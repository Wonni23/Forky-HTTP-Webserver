#ifndef EVENTLOOP_HPP
# define EVENTLOOP_HPP

#include "webserv.hpp"
#include <map>

class	Server;

class	EventLoop {
private:
	int							_epfd; // epoll fd
	std::map<int, u_int32_t>	_interests; // fd -> 이벤트 마스크
	int							_timeout_ms; // epoll_wait 타임아웃

	bool		ctl(int op, int fd, u_int32_t events); // epoll_clt()의 wrapper 함수
	static bool	setNonBlocking(int fd);

public:
	EventLoop();
	~EventLoop();

	bool	init(int timeout_ms = 1000);		// 1초 틱 기본
	bool	addServerSocket(int fd);			// EPOLLIN 등록
	bool	addClientSocket(int fd);			// EPOLLIN 등록
	bool	setWritable(int fd, bool enable);	// EPOLLOUT on/off
	bool	remove(int fd);						// epoll_ctl DEL

	void	run(Server& server);				// 단일 이벤트 루프
};

#endif