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
	int				_port;
	ClientState		_state;
	std::string		_raw_buffer;		//raw datas read from socket
	std::string		_response_buffer;
	HttpRequest*	_request;
	HttpResponse*	_response;
	size_t			_response_sent;
	time_t			_last_activity;
	
	void			setState(ClientState new_state);
	void			resetForNextRequest( void );

public:
	Client(int fd, int);
	~Client();

	// I/O 처리 (Client의 핵심 책임)
	bool			handleRead();
	bool			handleWrite();

	// 응답 설정 (외부에서 호출)
	void			setResponse(HttpResponse* response);

	// 상태 및 정보 조회
	int				getFd() const { return _fd; }
	int				getPort() const { return _port; }
	ClientState		getState() const { return _state; }
	HttpRequest*	getRequest() const { return _request; }

	// 유틸리티
	void			updateActivity();
	bool			isExpired(time_t now) const;
	bool			needsWriteEvent() const;
};

#endif // CLIENT_HPP
