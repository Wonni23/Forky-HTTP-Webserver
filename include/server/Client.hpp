#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "../webserv.hpp"

// Forward declarations
class HttpRequest;
class HttpResponse;

enum ClientState {
	READING_REQUEST,
	PROCESSING_REQUEST,
	WRITING_RESPONSE,
	KEEP_ALIVE,
	DISCONNECTED
};

class Client {
private:
	int				_fd;
	ClientState		_state;
	std::string		_raw_buffer;
	HttpRequest*	_current_request;
	HttpResponse*	_current_response;
	size_t			_response_sent;
	time_t			_last_activity;
	
	void			setState(ClientState new_state);

public:
	Client(int fd);
	~Client();

	// I/O 처리 (Client의 핵심 책임)
	bool			handleRead();
	bool			handleWrite();
	bool			isExpired(time_t now) const;

	// 상태 관리
	int				getFd() const { return _fd; }
	ClientState		getState() const { return _state; }
	void			updateActivity();

	// HTTP 객체 관리 (외부에서 라우팅 처리 후 사용)
	HttpRequest*	getCurrentRequest() const { return _current_request; }
	void			setResponse(HttpResponse* response);

	// EventLoop 연동
	bool			needsWriteEvent() const;
};

#endif
