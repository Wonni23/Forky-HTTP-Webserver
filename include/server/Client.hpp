#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "../webserv.hpp"

// Forward declarations
class HttpRequest;
class HttpResponse;
struct LocationContext;
struct ServerContext;

enum ClientState {
	READING_REQUEST,
	PROCESSING_REQUEST,
	WRITING_RESPONSE,
	KEEP_ALIVE,
	DISCONNECTED
};

enum ClientHeaderState {
	HEADER_INCOMPLETE,
	HEADER_COMPLETE,
	BODY_RECEIVING,
	REQUEST_COMPLETE
};

class Client {
private:
	int                     _fd;
	int                     _port;
	ClientState             _state;
	ClientHeaderState       _headerState;
	
	std::string             _raw_buffer;
	std::string             _response_buffer;
	HttpRequest*            _request;
	HttpResponse*           _response;
	size_t                  _response_sent;
	time_t                  _last_activity;

	size_t                  _headerEnd;
	const ServerContext*    _serverConf;
	const LocationContext*  _locConf;
	
	void                    setState(ClientState new_state);
	void                    resetForNextRequest(void);

	static const size_t MAX_REQUEST_SIZE;
	static const size_t MAX_HEADER_SIZE;

public:
	Client(int fd, int port);
	~Client();

	// I/O 처리
	bool                    handleWrite(void);

	// 헤더와 바디 파싱
	void                    appendRawBuffer(const char* data, size_t len);
	bool                    tryParseHeaders(void);
	bool                    tryParseBody(void);

	// 응답 설정
	void                    setResponse(HttpResponse* response);
	
	// 설정 관리
	void                    setServerContext(const ServerContext* conf);
	void                    setLocationContext(const LocationContext* conf);

	// 상태 및 정보 조회
	int                     getFd(void) const;
	int                     getPort(void) const;
	ClientState             getState(void) const;
	ClientHeaderState       getHeaderState(void) const;
	HttpRequest*            getRequest(void) const;
	const ServerContext*    getServerContext(void) const;
	const LocationContext*  getLocationContext(void) const;
	size_t                  getMaxBodySize(void) const;

	// 유틸리티
	void                    updateActivity(void);
	bool                    isExpired(time_t now) const;
	bool                    needsWriteEvent(void) const;
};

#endif // CLIENT_HPP
