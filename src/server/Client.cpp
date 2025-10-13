#include "server/Client.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/StatusCode.hpp"

// ========= 생성자 및 소멸자 =======
Client::Client(int fd, int port) 
	: _fd(fd), _port(port), _state(READING_REQUEST),
	_request(new HttpRequest()), _response(NULL), _response_sent(0) {
	updateActivity();
	DEBUG_LOG("Client created for fd=" << _fd << " on port " << _port);
}

Client::~Client() {
	delete _request;
	delete _response;
	DEBUG_LOG("Client destroyed for fd=" << _fd);
}

// ========== 상태 관리 ===========
void Client::updateActivity() {
	_last_activity = ::time(NULL);
}

void Client::setState(ClientState new_state) {
	DEBUG_LOG("Client fd=" << _fd << " state changed: " << _state << " -> " << new_state);
	_state = new_state;
}

bool Client::isExpired(time_t now) const {
	return (now - _last_activity) > CLIENT_TIMEOUT;
}

// ========== I/O 처리 ===========
bool Client::handleRead() {
	if (_state != READING_REQUEST) {
		return true;
	}
	
	char buffer[BUFFER_SIZE]; //8kb
	ssize_t bytes = ::recv(_fd, buffer, BUFFER_SIZE - 1, 0);
	
	updateActivity();
	
	if (bytes > 0) {
		buffer[bytes] = '\0';
		_raw_buffer.append(buffer, bytes);
		
		// 요청 크기 제한 체크
		if (_request && _request->isRequestTooLarge(_raw_buffer.size())) {
			ERROR_LOG("Request too large from fd=" << _fd);
			_response = new HttpResponse(HttpResponse::createErrorResponse(StatusCode::PAYLOAD_TOO_LARGE, NULL, NULL));	// 413 에러 응답 생성 (이것만 Client에서 직접 처리)
			setState(WRITING_RESPONSE);
			return true;
		}
		
		if (_request->parseRequest(_raw_buffer)) {
			DEBUG_LOG("Complete request received from fd=" << _fd);
			setState(PROCESSING_REQUEST); // 여기서는 단순히 상태만 변경, 라우팅은 외부에서 처리
		} else if (_request->getLastError() != HttpRequest::PARSE_INCOMPLETE) {
			ERROR_LOG("HTTP parsing failed for fd=" << _fd);
			int statusCode = _request->getStatusCodeForError();
			_response = new HttpResponse(HttpResponse::createErrorResponse(statusCode, NULL, NULL));
			setState(WRITING_RESPONSE);
		}
		
		return true;
		
	} else if (bytes == 0) {
		// 클라이언트가 연결을 정상적으로 종료
		DEBUG_LOG("Client fd=" << _fd << " closed connection");
		setState(DISCONNECTED);
		return false;
		
	} else {
		// bytes < 0: 논블로킹 소켓이서는 EAGAIN/EWOULDBLOCK 확인 필수
		// 이는 poll 후에 수행되므로 허용
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return true;
		}
		DEBUG_LOG("Read error from fd=" << _fd);
		setState(DISCONNECTED);
		return false;
	}
}

bool Client::handleWrite() {
	if (_state != WRITING_RESPONSE || !_response) {
		return true;
	}
	
	// HttpResponse에서 완전한 HTTP 응답 생성
	std::string response_data = _response->serialize(_request);
	
	size_t remaining = response_data.size() - _response_sent;
	ssize_t bytes = ::send(_fd, response_data.c_str() + _response_sent, remaining, 0);
	
	updateActivity();
	
	if (bytes > 0) {
		_response_sent += bytes;
		
		if (_response_sent >= response_data.size()) {
			DEBUG_LOG("Response sent completely to fd=" << _fd);
			
			// Keep-Alive 처리는 HttpRequest 객체를 통해
			if (_request && _request->isKeepAlive()) {
				// 다음 요청을 위해 초기화
				delete _request;
				delete _response;
				_request = 0;
				_response = 0;
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
	delete _response;
	_response = response;
	_response_sent = 0;
	setState(WRITING_RESPONSE);
}

bool Client::needsWriteEvent() const {
	return _state == WRITING_RESPONSE && _response != NULL;
}

void Client::resetForNextRequest() {
	_raw_buffer.clear();
	_response_sent = 0;

	delete _response;
	_response = 0;

	_request->reset();

	setState(READING_REQUEST);
}
