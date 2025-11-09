// Client.cpp - 핵심 디버그만 포함
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
const size_t Client::BUFFER_COMPACT_THRESHOLD = 1024 * 1024;


// ========= 생성자 및 소멸자 =======
Client::Client(int fd, int port)
	: _fd(fd), _port(port), _state(READING_REQUEST),
	_headerState(HEADER_INCOMPLETE),
	_request(new HttpRequest()), _response(NULL), _response_sent(0),
	_last_activity(0),
	_headerEnd(0),
	_serverConf(NULL),
	_locConf(NULL),
	_buffer_read_offset(0)
{
	updateActivity();
}


Client::~Client(void)
{
	delete _request;
	delete _response;
}


// ========= 버퍼 관리 =======
const char* Client::getBufferData() const
{
	return _raw_buffer.c_str() + _buffer_read_offset;
}


size_t Client::getBufferLength() const
{
	return _raw_buffer.length() - _buffer_read_offset;
}


void Client::consumeBuffer(size_t n)
{
	_buffer_read_offset += n;
	
	if (_buffer_read_offset > BUFFER_COMPACT_THRESHOLD) {
		compactBuffer();
	}
}


void Client::compactBuffer()
{
	if (_buffer_read_offset == 0) return;
	
	_raw_buffer.erase(0, _buffer_read_offset);
	_buffer_read_offset = 0;
}

// ========= 상태 관리 =======
void Client::updateActivity(void)
{
	_last_activity = ::time(NULL);
}


void Client::setState(ClientState new_state)
{
	_state = new_state;
}


bool Client::isExpired(time_t now) const
{
	if (_headerState == BODY_RECEIVING) return false;
	return (now - _last_activity) > CLIENT_TIMEOUT;
}


// ========= 접근자 =======
int Client::getFd(void) const { return _fd; }
int Client::getPort(void) const { return _port; }
ClientState Client::getState(void) const { return _state; }
ClientHeaderState Client::getHeaderState(void) const { return _headerState; }
HttpRequest* Client::getRequest(void) const { return _request; }
const ServerContext* Client::getServerContext(void) const { return _serverConf; }
const LocationContext* Client::getLocationContext(void) const { return _locConf; }


size_t Client::getMaxBodySize(void) const
{
	if (!_locConf || _locConf->opBodySizeDirective.empty()) {
		return 10UL * 1024 * 1024;
	}
	return StringUtils::toBytes(_locConf->opBodySizeDirective[0].size);
}


void Client::setServerContext(const ServerContext* conf) { _serverConf = conf; }
void Client::setLocationContext(const LocationContext* conf) { _locConf = conf; }
void Client::appendRawBuffer(const char* data, size_t len) { _raw_buffer.append(data, len); }
bool Client::needsWriteEvent(void) const { return _state == WRITING_RESPONSE && _response != NULL; }


// ========= I/O 처리 =======
bool Client::handleWrite(void)
{
	if (_state != WRITING_RESPONSE || !_response) return true;
	
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
			if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
			setState(DISCONNECTED);
			return false;
		}
	}
	
	// 에러 응답 처리
	if (_response) {
		int status = _response->getStatus();
		
		// 400 Bad Request / 405 Method Not Allowed / 5xx Server Error → 무조건 연결 종료
		if (status == 400 || status == 405 || status >= 500) {
			setState(DISCONNECTED);
			return false;
		}
		
		// 나머지 4xx (413, 404, 405 등) → keep-alive 확인
		if (status >= 400 && status < 500) {
			if (_request && _request->isKeepAlive()) {
				resetForNextRequest();
				return true;
			} else {
				setState(DISCONNECTED);
				return false;
			}
		}
	}
	
	// 2xx, 3xx 정상 응답 → keep-alive 확인
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
	
	size_t headerEnd = _raw_buffer.find("\r\n\r\n", _buffer_read_offset);
	
	if (headerEnd == std::string::npos) {
		if (getBufferLength() > MAX_HEADER_SIZE) {
			_response = new HttpResponse(
				HttpResponse::createErrorResponse(StatusCode::REQUEST_HEADER_FIELDS_TOO_LARGE, NULL, NULL)
			);
			_headerState = HEADER_COMPLETE;
			setState(WRITING_RESPONSE);
			return true;
		}
		return false;
	}
	
	std::string headerPart = _raw_buffer.substr(
		_buffer_read_offset, 
		headerEnd - _buffer_read_offset + 4
	);
	
	if (!_request->parseHeaders(headerPart)) {
		_response = new HttpResponse(
			HttpResponse::createErrorResponse(_request->getStatusCodeForError(), NULL, NULL)
		);
		_headerState = HEADER_COMPLETE;
		setState(WRITING_RESPONSE);
		return true;
	}
	
	_headerEnd = headerEnd + 4;
	_headerState = HEADER_COMPLETE;
	return true;
}


// ========= Body 파싱 =======
bool Client::tryParseBody(void)
{
	// 1. 초기 상태 검사
	if (_headerState != HEADER_COMPLETE && _headerState != BODY_RECEIVING) {
		return false;
	}
	if (_state == PROCESSING_REQUEST) return true;
	
	// 2. 공통 변수 설정
	size_t bodyStart = _headerEnd;
	size_t currentBodyLength = _raw_buffer.length() - bodyStart;
	size_t maxBodySize = getMaxBodySize();
	
	bool isChunked = _request->isChunkedEncoding();
	size_t expectedBodyLength = isChunked ? 0 : _request->getContentLength();
	
	// 3. Body가 없는 요청 처리 (공통)
	if (expectedBodyLength == 0 && !isChunked) {
		consumeBuffer(bodyStart - _buffer_read_offset); // 헤더 부분만 소비
		_lastBodyLength = 0;
		_headerState = REQUEST_COMPLETE;
		setState(PROCESSING_REQUEST);
		return true;
	}
	
	// 4. Body가 있지만 아직 받지 못한 경우 
	if (currentBodyLength == 0) {
		_headerState = BODY_RECEIVING;
		return false; // 더 많은 데이터 필요
	}
	
	// 5. 전문 함수에게 위임
	if (isChunked) {
		return tryParseChunkedBody(bodyStart, maxBodySize);
	} else {
		return tryParseContentLengthBody(bodyStart, maxBodySize, expectedBodyLength);
	}
}


bool Client::tryParseChunkedBody(size_t bodyStart, size_t maxBodySize)
{
	size_t bufLen = _raw_buffer.length() - bodyStart;
	
	if (bufLen < 5) { // "0\r\n\r\n"
		_headerState = BODY_RECEIVING;
		return false;
	}

	if (bufLen == 5 && _raw_buffer.substr(bodyStart, 5) == "0\r\n\r\n") {
		// 빈 body
		_request->setDecodedBody("");
		_lastBodyLength = 0;
		_raw_buffer.clear();
		_buffer_read_offset = 0;
		_headerState = REQUEST_COMPLETE;
		setState(PROCESSING_REQUEST);
		return true;
	}
	
	// 마지막 200바이트만 검색
	size_t searchStart = (bufLen > 200) ? (bodyStart + bufLen - 200) : bodyStart;
	
	size_t lastChunkPos = _raw_buffer.find("0\r\n", searchStart);
	if (lastChunkPos == std::string::npos) {
		_headerState = BODY_RECEIVING;
		return false;
	}
	
	size_t finalCrlfPos = _raw_buffer.find("\r\n\r\n", lastChunkPos);
	if (finalCrlfPos == std::string::npos) {
		_headerState = BODY_RECEIVING;
		return false;
	}
	
	// 디코딩 (한 번만)
	std::string bodyPart = _raw_buffer.substr(bodyStart);
	std::string decodedBody = _request->decodeChunkedBody(bodyPart);
	
	if (decodedBody.empty() && bodyPart.length() > 5) {
		_response = new HttpResponse(
			HttpResponse::createErrorResponse(StatusCode::BAD_REQUEST, _serverConf, _locConf)
		);
		setState(WRITING_RESPONSE);
		_raw_buffer.clear();
		_buffer_read_offset = 0;
		return true;
	}
	
	if (decodedBody.length() > maxBodySize) {
		_response = new HttpResponse(
			HttpResponse::createErrorResponse(StatusCode::PAYLOAD_TOO_LARGE, _serverConf, _locConf)
		);
		setState(WRITING_RESPONSE);
		_raw_buffer.clear();
		_buffer_read_offset = 0;
		return true;
	}
	
	_request->setDecodedBody(decodedBody);
	_lastBodyLength = decodedBody.length();
	
	_raw_buffer.clear();
	_buffer_read_offset = 0;
	
	_headerState = REQUEST_COMPLETE;
	setState(PROCESSING_REQUEST);
	return true;
}


bool Client::tryParseContentLengthBody(size_t bodyStart, size_t maxBodySize, size_t expectedBodyLength)
{
	// Content-Length
	if (expectedBodyLength > maxBodySize) {
		_response = new HttpResponse(
			HttpResponse::createErrorResponse(StatusCode::PAYLOAD_TOO_LARGE, _serverConf, _locConf)
		);
		setState(WRITING_RESPONSE);
		return true; // 파싱 완료 (실패)
	}
	
	size_t currentBodyLength = _raw_buffer.length() - bodyStart;
	
	if (currentBodyLength < expectedBodyLength) {
		_headerState = BODY_RECEIVING;
		return false; // 더 많은 데이터 필요
	}
	
	// Zero-Copy
	_request->setBodyReference(&_raw_buffer, bodyStart, expectedBodyLength);
	_lastBodyLength = expectedBodyLength;
	
	_headerState = REQUEST_COMPLETE;
	setState(PROCESSING_REQUEST);
	return true; // 파싱 완료 (성공)
}


// ========= 다음 요청 준비 =======
void Client::resetForNextRequest(void)
{
	delete _response;
	_response = NULL;
	delete _request;
	_request = new HttpRequest();
	
	if (getBufferLength() == 0) {
		_raw_buffer.clear();
		_buffer_read_offset = 0;
	} else {
		if (_buffer_read_offset > BUFFER_COMPACT_THRESHOLD / 2) {
			compactBuffer();
		}
	}
	
	_response_buffer.clear();
	_response_sent = 0;
	_headerEnd = 0;
	_headerState = HEADER_INCOMPLETE;
	_serverConf = NULL;
	_locConf = NULL;
	setState(READING_REQUEST);
}
