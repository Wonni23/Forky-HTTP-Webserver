#include "server/Client.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/RequestRouter.hpp"
#include "http/HttpController.hpp"
#include "http/StatusCode.hpp"
#include "utils/StringUtils.hpp"
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>

// ========= 정적 상수 정의 =======
const size_t Client::MAX_REQUEST_SIZE = 10UL * 1024 * 1024 * 1024;
const size_t Client::MAX_HEADER_SIZE = 8192;

// ========= 생성자 및 소멸자 =======
Client::Client(int fd, int port)
	: _fd(fd), _port(port), _state(READING_REQUEST),
	  _headerState(HEADER_INCOMPLETE),
	  _request(new HttpRequest()), _response(NULL), _response_sent(0),
	  _last_activity(0),
	  _headerEnd(0),
	  _serverConf(NULL),
	  _locConf(NULL)
{
	updateActivity();
	DEBUG_LOG("[Client] fd=" << _fd << " created on port=" << _port);
}

Client::~Client(void)
{
	delete _request;
	delete _response;
	DEBUG_LOG("[Client] fd=" << _fd << " destroyed");
}

// ========= 상태 관리 =======
void Client::updateActivity(void)
{
	_last_activity = ::time(NULL);
}

void Client::setState(ClientState new_state)
{
	DEEP_LOG("[Client] fd=" << _fd << " state: " << _state << " -> " << new_state);
	_state = new_state;
}

bool Client::isExpired(time_t now) const
{
	if (_headerState == BODY_RECEIVING) {
		return false;
	}
	return (now - _last_activity) > CLIENT_TIMEOUT;
}

// ========= 접근자 =======
int Client::getFd(void) const
{
	return _fd;
}

int Client::getPort(void) const
{
	return _port;
}

ClientState Client::getState(void) const
{
	return _state;
}

ClientHeaderState Client::getHeaderState(void) const
{
	return _headerState;
}

HttpRequest* Client::getRequest(void) const
{
	return _request;
}

const ServerContext* Client::getServerContext(void) const
{
	return _serverConf;
}

const LocationContext* Client::getLocationContext(void) const
{
	return _locConf;
}

size_t Client::getMaxBodySize(void) const
{
	if (!_locConf || _locConf->opBodySizeDirective.empty()) {
		return 10UL * 1024 * 1024;
	}

	size_t bodySize = StringUtils::toBytes(_locConf->opBodySizeDirective[0].size);
	
	DEBUG_LOG("[Client] fd=" << _fd << " client_max_body_size: " 
			<< _locConf->opBodySizeDirective[0].size << " = " << bodySize << " bytes");

	return bodySize;
}

// ========= 설정 관리 =======
void Client::setServerContext(const ServerContext* conf)
{
	_serverConf = conf;
}

void Client::setLocationContext(const LocationContext* conf)
{
	_locConf = conf;
}

void Client::appendRawBuffer(const char* data, size_t len)
{
	_raw_buffer.append(data, len);
}

bool Client::needsWriteEvent(void) const
{
	return _state == WRITING_RESPONSE && _response != NULL;
}

// ========= I/O 처리 =======
bool Client::handleWrite(void)
{
	if (_state != WRITING_RESPONSE || !_response) {
		return true;
	}

	if (_response_buffer.empty()) {
		_response_buffer = _response->serialize(_request);
	}

	while (_response_sent < _response_buffer.size()) {
		size_t remaining = _response_buffer.size() - _response_sent;
		ssize_t bytes = ::send(_fd, _response_buffer.c_str() + _response_sent, remaining, 0);

		updateActivity();

		if (bytes > 0) {
			_response_sent += bytes;
		} else if (bytes == 0) {
			setState(DISCONNECTED);
			return false;
		} else {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return true;
			}
			ERROR_LOG("[Client] fd=" << _fd << " write error: " << std::strerror(errno));
			setState(DISCONNECTED);
			return false;
		}
	}

	DEBUG_LOG("[Client] fd=" << _fd << " response sent completely");

	// ✅ 개선된 에러 응답 처리
	if (_response && _response->getStatus() >= 400) {
		// Connection: close 헤더 확인
		std::string connHeader = _response->getHeader("Connection");
		
		// 5xx 서버 에러는 무조건 연결 종료
		if (_response->getStatus() >= 500) {
			DEBUG_LOG("[Client] fd=" << _fd << " server error (5xx), closing");
			setState(DISCONNECTED);
			return false;
		}
		
		// 4xx 클라이언트 에러는 Connection 헤더 확인
		if (connHeader == "close") {
			DEBUG_LOG("[Client] fd=" << _fd << " client error (4xx) with Connection: close");
			setState(DISCONNECTED);
			return false;
		}
		
		// Connection: close가 없고 Keep-alive 가능하면 연결 유지
		DEBUG_LOG("[Client] fd=" << _fd << " client error (4xx) but keeping connection alive");
	}

	// Keep-alive 처리
	if (_request && _request->isKeepAlive()) {
		resetForNextRequest();
	} else {
		setState(DISCONNECTED);
		return false;
	}

	return true;
}


void Client::setResponse(HttpResponse* response)
{
	DEBUG_LOG("[Client] fd=" << _fd << " setting response");
	delete _response;
	_response = response;
	_response_sent = 0;
	setState(WRITING_RESPONSE);
}

// ========= 헤더 파싱 =======
bool Client::tryParseHeaders(void)
{
	if (_headerState != HEADER_INCOMPLETE) {
		return _headerState == HEADER_COMPLETE;
	}

	size_t headerEnd = _raw_buffer.find("\r\n\r\n");
	if (headerEnd == std::string::npos) {
		if (_raw_buffer.length() > MAX_HEADER_SIZE) {
			ERROR_LOG("[Client] fd=" << _fd << " header too large");
			_response = new HttpResponse(
				HttpResponse::createErrorResponse(StatusCode::REQUEST_HEADER_FIELDS_TOO_LARGE, NULL, NULL)
			);
			_headerState = HEADER_COMPLETE;  // ← 상태 변경 추가
			setState(WRITING_RESPONSE);
			return true;
		}
		DEBUG_LOG("[Client] fd=" << _fd << " waiting for header end, buffer size: "
				<< _raw_buffer.length());
		return false;
	}

	std::string headerPart = _raw_buffer.substr(0, headerEnd + 4);

	if (!_request->parseHeaders(headerPart)) {
		ERROR_LOG("[Client] fd=" << _fd << " header parse failed");
		_response = new HttpResponse(
			HttpResponse::createErrorResponse(_request->getStatusCodeForError(), NULL, NULL)
		);
		_headerState = HEADER_COMPLETE;  // ← 상태 변경 추가
		setState(WRITING_RESPONSE);
		return true;
	}

	DEBUG_LOG("[Client] fd=" << _fd << " headers parsed: "
			<< _request->getMethod() << " " << _request->getUri());

	_headerEnd = headerEnd + 4;
	_headerState = HEADER_COMPLETE;
	return true;
}


// ========= Body 파싱 =======
bool Client::tryParseBody(void)
{
	if (_headerState != HEADER_COMPLETE && _headerState != BODY_RECEIVING) {
		return false;
	}

	if (_state == PROCESSING_REQUEST) {
		return true;
	}

	size_t bodyStart = _headerEnd;
	size_t currentBodyLength = _raw_buffer.length() - bodyStart;
	size_t maxBodySize = getMaxBodySize();

	bool isChunked = _request->isChunkedEncoding();
	size_t expectedBodyLength = isChunked ? 0 : _request->getContentLength();

	DEBUG_LOG("[Client] fd=" << _fd << " tryParseBody: isChunked=" << (isChunked ? "YES" : "NO")
			<< " expected=" << expectedBodyLength << " current=" << currentBodyLength);

	// ========= Body 없음: 즉시 완료! ==========
	if (expectedBodyLength == 0 && !isChunked) {
		DEBUG_LOG("[Client] fd=" << _fd << " no body needed - complete");
		_headerState = REQUEST_COMPLETE;
		setState(PROCESSING_REQUEST);
		return true;
	}

	// ========= Body 데이터 아직 안 옴: 대기 ==========
	if (currentBodyLength == 0) {
		DEBUG_LOG("[Client] fd=" << _fd << " no body data yet");
		_headerState = BODY_RECEIVING;
		return false;
	}

	// ========= Chunked 인코딩 ==========
	if (isChunked) {
		std::string bodyPart = _raw_buffer.substr(bodyStart);
		
		DEBUG_LOG("[Client] fd=" << _fd << " checking chunked completion, bodyPart length=" 
				<< bodyPart.length());
		
		// 디버깅: 마지막 100바이트 시각화
		if (bodyPart.length() > 0) {
			size_t showLen = bodyPart.length() > 100 ? 100 : bodyPart.length();
			size_t startPos = bodyPart.length() - showLen;
			std::string tail = bodyPart.substr(startPos);
			
			std::string visualized;
			for (size_t i = 0; i < tail.length(); ++i) {
				char c = tail[i];
				if (c == '\r')
					visualized += "\\r";
				else if (c == '\n')
					visualized += "\\n";
				else if (c == '\0')
					visualized += "\\0";
				else if (c < 32 || c >= 127) {
					char buf[8];
					std::snprintf(buf, sizeof(buf), "\\x%02X", (unsigned char)c);
					visualized += buf;
				}
				else
					visualized += c;
			}
			
			DEBUG_LOG("[Client] fd=" << _fd << " last " << showLen 
					<< " bytes: [" << visualized << "]");
		}
		
		// 1단계: 0 크기 청크 찾기
		size_t lastChunkPos = bodyPart.find("0\r\n");
		
		if (lastChunkPos == std::string::npos) {
			DEBUG_LOG("[Client] fd=" << _fd << " chunked incomplete (no 0\\r\\n found)");
			_headerState = BODY_RECEIVING;
			return false;
		}
		
		DEBUG_LOG("[Client] fd=" << _fd << " found 0\\r\\n at pos=" << lastChunkPos);
		
		// 2단계: 0\r\n 이후 최종 CRLF 찾기 (trailer 고려)
		size_t finalCrlfPos = bodyPart.find("\r\n\r\n", lastChunkPos);
		
		if (finalCrlfPos == std::string::npos) {
			DEBUG_LOG("[Client] fd=" << _fd << " chunked incomplete (no final \\r\\n\\r\\n after 0\\r\\n)");
			_headerState = BODY_RECEIVING;
			return false;
		}
		
		DEBUG_LOG("[Client] fd=" << _fd << " found final \\r\\n\\r\\n at pos=" << finalCrlfPos);
		DEBUG_LOG("[Client] fd=" << _fd << " decoding chunked...");
		
		std::string decodedBody = _request->decodeChunkedBody(bodyPart);
		
		// 디코딩 실패 체크
		if (decodedBody.empty() && bodyPart.length() > 5) {
			ERROR_LOG("[Client] fd=" << _fd << " chunked decode failed - malformed data");
			_response = new HttpResponse(
				HttpResponse::createErrorResponse(StatusCode::BAD_REQUEST, 
					_serverConf, _locConf)
			);
			setState(WRITING_RESPONSE);
			return true;
		}
		
		DEBUG_LOG("[Client] fd=" << _fd << " decoded: " << decodedBody.length() << " bytes");

		if (decodedBody.length() > maxBodySize) {
			ERROR_LOG("[Client] fd=" << _fd << " body too large");
			_response = new HttpResponse(
				HttpResponse::createErrorResponse(StatusCode::PAYLOAD_TOO_LARGE, 
					_serverConf, _locConf)
			);
			setState(WRITING_RESPONSE);
			return true;
		}

		_request->setDecodedBody(decodedBody);
		_raw_buffer.clear();
		_headerState = REQUEST_COMPLETE;
		setState(PROCESSING_REQUEST);
		return true;
	}

	// ========= Content-Length 기반 (메모리 처리) ==========
	
	// 크기 체크 먼저
	if (expectedBodyLength > maxBodySize) {
		ERROR_LOG("[Client] fd=" << _fd << " body size exceeds limit: " 
				<< expectedBodyLength << " > " << maxBodySize);
		_response = new HttpResponse(
			HttpResponse::createErrorResponse(StatusCode::PAYLOAD_TOO_LARGE, 
				_serverConf, _locConf)
		);
		setState(WRITING_RESPONSE);
		return true;
	}

	// 아직 덜 받음
	if (currentBodyLength < expectedBodyLength) {
		_headerState = BODY_RECEIVING;
		DEBUG_LOG("[Client] fd=" << _fd << " body incomplete: "
				<< currentBodyLength << "/" << expectedBodyLength);
		return false;
	}

	// ========= Body 완성! ==========
	std::string body = _raw_buffer.substr(bodyStart, expectedBodyLength);
	_request->setDecodedBody(body);
	
	DEBUG_LOG("[Client] fd=" << _fd << " body complete: " << body.length() << " bytes");

	// 사용한 데이터 제거 (pipelined requests 대비)
	if (bodyStart + expectedBodyLength < _raw_buffer.length()) {
		// 뒤에 데이터가 더 있음 (pipelined request)
		_raw_buffer.erase(0, bodyStart + expectedBodyLength);
		DEBUG_LOG("[Client] fd=" << _fd << " pipelined data remaining: " 
				<< _raw_buffer.length() << " bytes");
	} else {
		// 다 사용했음
		_raw_buffer.clear();
	}

	_headerState = REQUEST_COMPLETE;
	setState(PROCESSING_REQUEST);
	return true;
}

// ========= 다음 요청 준비 =======
void Client::resetForNextRequest(void)
{
	DEBUG_LOG("[Client] fd=" << _fd << " resetting for next request");

	delete _response;
	_response = NULL;

	delete _request;
	_request = new HttpRequest();

	_raw_buffer.clear();
	_response_buffer.clear();
	_response_sent = 0;

	_headerEnd = 0;
	_headerState = HEADER_INCOMPLETE;

	_serverConf = NULL;
	_locConf = NULL;

	setState(READING_REQUEST);
}
