#include "server/Client.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/StatusCode.hpp"

Client::Client(int fd) 
	: _fd(fd), _state(READING_REQUEST), _current_request(NULL),
	  _current_response(NULL), _response_sent(0) {
	updateActivity();
	DEBUG_LOG("Client created for fd=" << _fd);
}

Client::~Client() {
	delete _current_request;
	delete _current_response;
	DEBUG_LOG("Client destroyed for fd=" << _fd);
}

void Client::updateActivity() {
	_last_activity = ::time(NULL);
}

void Client::setState(ClientState new_state) {
	DEBUG_LOG("Client fd=" << _fd << " state change: " << _state << " -> " << new_state);
	_state = new_state;
}

bool Client::isExpired(time_t now) const {
	return (now - _last_activity) > CLIENT_TIMEOUT;
}

bool Client::handleRead() {
	if (_state != READING_REQUEST) {
		return true;
	}
	
	char buffer[BUFFER_SIZE];
	ssize_t bytes = ::recv(_fd, buffer, BUFFER_SIZE - 1, 0);
	
	updateActivity();
	
	if (bytes > 0) {
		buffer[bytes] = '\0';
		_raw_buffer.append(buffer, bytes);
		
		// 요청 크기 제한 체크
		if (_raw_buffer.size() > MAX_REQUEST_SIZE) {
			ERROR_LOG("Request too large from fd=" << _fd);
			
			// 413 에러 응답 생성 (이것만 Client에서 직접 처리)
			_current_response = new HttpResponse();
			_current_response->setStatusCode(StatusCode::REQUEST_ENTITY_TOO_LARGE);
			_current_response->setBody(StatusCodeUtils::getDefaultErrorPage(StatusCode::REQUEST_ENTITY_TOO_LARGE));
			setState(WRITING_RESPONSE);
			return true;
		}
		
		// HTTP 요청 파싱 시도 (HttpRequest에 파싱 기능 통합됨)
		if (!_current_request) {
			_current_request = new HttpRequest();
		}
		
		if (_current_request->parseFrom(_raw_buffer)) {
			DEBUG_LOG("Complete request received from fd=" << _fd);
			// 여기서는 단순히 상태만 변경, 라우팅은 외부에서 처리
			setState(PROCESSING_REQUEST);
		}
		
		return true;
		
	} else if (bytes == 0) {
		// 클라이언트가 연결을 정상적으로 종료
		DEBUG_LOG("Client fd=" << _fd << " closed connection");
		setState(DISCONNECTED);
		return false;
		
	} else {
		// bytes < 0: 에러 발생 (errno 체크 금지에 따라 즉시 연결 종료)
		DEBUG_LOG("Read error from fd=" << _fd);
		setState(DISCONNECTED);
		return false;
	}
}

bool Client::handleWrite() {
	if (_state != WRITING_RESPONSE || !_current_response) {
		return true;
	}
	
	// HttpResponse에서 완전한 HTTP 응답 생성
	std::string response_data = _current_response->serialize();
	
	size_t remaining = response_data.size() - _response_sent;
	ssize_t bytes = ::send(_fd, response_data.c_str() + _response_sent, remaining, 0);
	
	updateActivity();
	
	if (bytes > 0) {
		_response_sent += bytes;
		
		if (_response_sent >= response_data.size()) {
			DEBUG_LOG("Response sent completely to fd=" << _fd);
			
			// Keep-Alive 처리는 HttpRequest 객체를 통해
			if (_current_request && _current_request->isKeepAlive()) {
				// 다음 요청을 위해 초기화
				delete _current_request;
				delete _current_response;
				_current_request = nullptr;
				_current_response = nullptr;
				_raw_buffer.clear();
				_response_sent = 0;
				setState(READING_REQUEST);
			} else {
				setState(DISCONNECTED);
				return false; // 연결 종료
			}
		}
		
		return true;
		
	} else if (bytes == 0) {
		// 전송할 수 없음 (일반적으로 발생하지 않음)
		DEBUG_LOG("Write returned 0 for fd=" << _fd);
		setState(DISCONNECTED);
		return false;
		
	} else {
		// bytes < 0: 에러 발생
		DEBUG_LOG("Write error for fd=" << _fd);
		setState(DISCONNECTED);
		return false;
	}
}

void Client::setResponse(HttpResponse* response) {
	delete _current_response;
	_current_response = response;
	_response_sent = 0;
	setState(WRITING_RESPONSE);
}

bool Client::needsWriteEvent() const {
	return _state == WRITING_RESPONSE && _current_response != nullptr;
}
