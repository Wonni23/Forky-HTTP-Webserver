#include "server/EventLoop.hpp"
#include "server/Server.hpp"

EventLoop::EventLoop() : _epfd(-1), _timeout_ms(1000) {}
EventLoop::~EventLoop() { if (_epfd != -1) ::close(_epfd); }

bool EventLoop::init(int timeout_ms) {
	_timeout_ms = timeout_ms;
	_epfd = ::epoll_create(MAX_EVENTS);
	return _epfd != -1;
}

bool EventLoop::ctl(int op, int fd, u_int32_t events) {
	struct epoll_event ev;
	std::memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.fd = fd;
	if (::epoll_ctl(_epfd, op, fd, &ev) == -1) return false;
	if (op == EPOLL_CTL_DEL) _interests.erase(fd);
	else _interests[fd] = events;
	return true;
}

void EventLoop::setNonBlocking(int fd) {
	int flags = ::fcntl(fd, F_GETFL, 0);
	if (flags == -1) flags = 0;
	::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool EventLoop::addServerSocket(int fd) {
	setNonBlocking(fd);
	return ctl(EPOLL_CTL_ADD, fd, EPOLLIN);
}

bool EventLoop::addClientSocket(int fd) {
	setNonBlocking(fd);
	return ctl(EPOLL_CTL_ADD, fd, EPOLLIN); // 처음엔 읽기만
}

bool EventLoop::setWritable(int fd, bool enable) {
	std::map<int, uint32_t>::iterator it = _interests.find(fd);
	if (it == _interests.end()) return false;
	uint32_t ev = it->second;
	if (enable)	ev |= EPOLLOUT;
	else		ev &= ~EPOLLOUT;
	return ctl(EPOLL_CTL_MOD, fd, ev);
}

bool EventLoop::remove(int fd) {
	// DEL 실패해도 close 진행은 상위에서 처리
	ctl(EPOLL_CTL_DEL, fd, 0);
	return true;
}

void EventLoop::run(Server& server) {
	struct epoll_event events[MAX_EVENTS];

	while (true) {
		int n = ::epoll_wait(_epfd, events, MAX_EVENTS, _timeout_ms);
		if (n < 0) {
			// 루프 유지(규정상 서버는 쉽게 죽지 않아야 함)
			continue;
		}
		for (int i = 0; i < n; ++i) {
			int fd = events[i].data.fd;
			uint32_t ev = events[i].events;

			if (ev & (EPOLLERR | EPOLLHUP)) {
				server.onHangup(fd);
				continue;
			}
			if (ev & EPOLLIN) {
				server.onReadable(fd);
			}
			if (ev & EPOLLOUT) {
				server.onWritable(fd);
			}
		}
		// 주기적 점검(타임아웃, 만료 연결 정리 등)
		server.onTick();
	}
}