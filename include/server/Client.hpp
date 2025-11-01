// include/server/Client.hpp
#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "../webserv.hpp"

class HttpRequest;
class HttpResponse;
struct ServerContext;
struct LocationContext;

enum ClientState {
	READING_REQUEST,
	PROCESSING_REQUEST,
	WRITING_RESPONSE,
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
	int					_fd;
	int					_port;
	ClientState			_state;
	ClientHeaderState	_headerState;
	
	// Buffer Index 방식 추가
	std::string			_raw_buffer;
	size_t				_buffer_read_offset;  // 🆕 읽은 데이터의 오프셋
	std::string			_response_buffer;
	size_t				_lastBodyLength;
	
	HttpRequest*		_request;
	HttpResponse*		_response;
	size_t				_response_sent;
	time_t				_last_activity;
	size_t				_headerEnd;
	
	const ServerContext*	_serverConf;
	const LocationContext*	_locConf;
	
	
	// 🆕 버퍼 관리 헬퍼 메서드
	const char*			getBufferData() const;
	size_t				getBufferLength() const;
	void				consumeBuffer(size_t n);
	void				compactBuffer();  // 주기적 버퍼 정리
	
	// 기존 메서드들
	void				setState(ClientState new_state);
	void				resetForNextRequest(void);
	
public:
	static const size_t MAX_REQUEST_SIZE;
	static const size_t MAX_HEADER_SIZE;
	static const size_t BUFFER_COMPACT_THRESHOLD;  // 🆕 버퍼 정리 임계값
	
	Client(int fd, int port);
	~Client(void);
	
	// I/O 처리
	bool				tryParseHeaders(void);
	bool				tryParseBody(void);
	bool				handleWrite(void);
	void				setResponse(HttpResponse* response);
	
	// 상태 조회
	int					getFd(void) const;
	int					getPort(void) const;
	ClientState			getState(void) const;
	ClientHeaderState	getHeaderState(void) const;
	HttpRequest*		getRequest(void) const;
	const ServerContext* getServerContext(void) const;
	const LocationContext* getLocationContext(void) const;
	size_t				getMaxBodySize(void) const;
	
	// 설정 관리
	void				setServerContext(const ServerContext* conf);
	void				setLocationContext(const LocationContext* conf);
	void				appendRawBuffer(const char* data, size_t len);
	void				updateActivity(void);
	bool				isExpired(time_t now) const;
	bool				needsWriteEvent(void) const;
};

#endif
