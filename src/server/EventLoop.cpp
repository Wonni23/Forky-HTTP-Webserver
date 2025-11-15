#include "server/EventLoop.hpp"
#include "server/Server.hpp"


EventLoop::EventLoop() : _epfd(-1), _timeout_ms(1000) {}


EventLoop::~EventLoop() { 
	if (_epfd != -1) {
		::close(_epfd);
	}
}


bool EventLoop::init(int timeout_ms) {
	_timeout_ms = timeout_ms;
	_epfd = ::epoll_create(MAX_EVENTS);
	
	if (_epfd == -1) {
		ERROR_LOG("[EventLoop] epoll_create failed: " << std::strerror(errno));
		return false;
	}
	
	DEBUG_LOG("[EventLoop] initialized with epfd=" << _epfd << " timeout=" << _timeout_ms << "ms");
	return true;
}


bool EventLoop::ctl(int op, int fd, uint32_t events) {
	struct epoll_event ev = {};
	ev.events = events;
	ev.data.fd = fd;
	
	// const char* op_str = (op == EPOLL_CTL_ADD) ? "ADD" : (op == EPOLL_CTL_MOD) ? "MOD" : "DEL";
	
	if (::epoll_ctl(_epfd, op, fd, &ev) == -1) {
		// ERROR_LOG("[EventLoop] epoll_ctl " << op_str << " failed for fd=" << fd << ": " << std::strerror(errno));
		return false;
	}
	
	if (op == EPOLL_CTL_DEL) {
		_interests.erase(fd);
	} else {
		_interests[fd] = events;
	}
	
	return true;
}


bool EventLoop::setNonBlocking(int fd) {
	int flags = ::fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		ERROR_LOG("[EventLoop] fcntl(F_GETFL) failed for fd=" << fd << ": " << std::strerror(errno));
		flags = 0;
	}
	
	if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		ERROR_LOG("[EventLoop] fcntl(F_SETFL) failed for fd=" << fd << ": " << std::strerror(errno));
		return false;
	}
	
	return true;
}


bool EventLoop::addServerSocket(int fd) {
	if (!setNonBlocking(fd)) {
		return false;
	}
	return ctl(EPOLL_CTL_ADD, fd, EPOLLIN);
}


bool EventLoop::addClientSocket(int fd) {
	if (!setNonBlocking(fd)) {
		return false;
	}
	return ctl(EPOLL_CTL_ADD, fd, EPOLLIN | EPOLLRDHUP);
}


bool EventLoop::setWritable(int fd, bool enable) {
	std::map<int, uint32_t>::iterator it = _interests.find(fd);
	if (it == _interests.end()) {
		ERROR_LOG("[EventLoop] fd=" << fd << " not found in interests map");
		return false;
	}
	
	uint32_t old_events = it->second;
	uint32_t new_events;
	
	if (enable) {
		new_events = old_events | EPOLLOUT;
	} else {
		new_events = old_events & ~EPOLLOUT;
	}
	
	if (old_events == new_events) {
		return true;  // 변경 없음
	}
	
	return ctl(EPOLL_CTL_MOD, fd, new_events);
}


bool EventLoop::remove(int fd) {
	std::map<int, uint32_t>::iterator it = _interests.find(fd);
	if (it == _interests.end()) {
		return true;  // 이미 제거됨
	}
	
	return ctl(EPOLL_CTL_DEL, fd, 0);
}


void EventLoop::run(Server& server) {
	struct epoll_event events[MAX_EVENTS];
	INFO_LOG("[EventLoop] started with timeout=" << _timeout_ms << "ms");


	while (true) {
		int n = ::epoll_wait(_epfd, events, MAX_EVENTS, _timeout_ms);
		
		if (n < 0) {
			if (errno == EINTR) {
				continue;  // 신호 인터럽트는 정상
			}
			ERROR_LOG("[EventLoop] epoll_wait failed: " << std::strerror(errno));
			break;
		}
		
		for (int i = 0; i < n; ++i) {
			int fd = events[i].data.fd;
			uint32_t ev = events[i].events;

			// EPOLLERR는 진짜 에러이므로 즉시 종료
			if (ev & EPOLLERR) {
				DEBUG_LOG("[EventLoop] fd=" << fd << " error detected");
				server.onHangup(fd);
				continue;
			}

			// EPOLLIN: 읽을 데이터가 있으면 먼저 읽기
			if (ev & EPOLLIN) {
				server.onReadable(fd);
			}

			// EPOLLOUT: 쓸 수 있으면 쓰기
			if (ev & EPOLLOUT) {
				server.onWritable(fd);
			}

			// EPOLLHUP/EPOLLRDHUP: 읽기/쓰기 후 연결 종료 처리
			if (ev & (EPOLLHUP | EPOLLRDHUP)) {
				DEBUG_LOG("[EventLoop] fd=" << fd << " hangup detected");
				server.onHangup(fd);
			}
		}
		
		server.onTick();
	}
	
	INFO_LOG("[EventLoop] terminated");
}
