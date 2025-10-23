#include "server/Client.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/StatusCode.hpp"


// ========= 생성자 및 소멸자 =======
Client::Client(int fd, int port) 
	: _fd(fd), _port(port), _state(READING_REQUEST),
	_request(new HttpRequest()), _response(NULL), _response_sent(0) {
	updateActivity();
	DEBUG_LOG("[Client] fd=" << _fd << " created on port=" << _port);
}


Client::~Client() {
	delete _request;
	delete _response;
	DEBUG_LOG("[Client] fd=" << _fd << " destroyed");
}


// ========== 상태 관리 ===========
void Client::updateActivity() {
	_last_activity = ::time(NULL);
}


void Client::setState(ClientState new_state) {
	DEEP_LOG("[Client] fd=" << _fd << " state changed: " << _state << " -> " << new_state);
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
	
	if (_request == NULL) {
		DEBUG_LOG("[Client] fd=" << _fd << " _request is NULL, creating new one");
		_request = new HttpRequest();
	}


	char buffer[BUFFER_SIZE]; //8kb
	ssize_t bytes = ::recv(_fd, buffer, BUFFER_SIZE - 1, 0);
	
	updateActivity();
	
	if (bytes > 0) {
		buffer[bytes] = '\0';
		_raw_buffer.append(buffer, bytes);
		DEEP_LOG("[Client] fd=" << _fd << " read " << bytes << " bytes, buffer_size=" << _raw_buffer.size());
		
		// 요청 크기 제한 체크
		if (_request && _request->isRequestTooLarge(_raw_buffer.size())) {
			ERROR_LOG("[Client] fd=" << _fd << " request too large: " << _raw_buffer.size() << " bytes");
			_response = new HttpResponse(HttpResponse::createErrorResponse(StatusCode::PAYLOAD_TOO_LARGE, NULL, NULL));
			setState(WRITING_RESPONSE);
			return true;
		}
		
		if (_request->parseRequest(_raw_buffer)) {
			DEBUG_LOG("[Client] fd=" << _fd << " complete request received");
			_raw_buffer.clear();
			setState(PROCESSING_REQUEST);
		} else if (_request->getLastError() != HttpRequest::PARSE_INCOMPLETE) {
			ERROR_LOG("[Client] fd=" << _fd << " HTTP parsing failed, error=" << _request->getLastError());
			int statusCode = _request->getStatusCodeForError(); //체크
			_response = new HttpResponse(HttpResponse::createErrorResponse(statusCode, NULL, NULL));
			setState(WRITING_RESPONSE);
		} else {
			DEEP_LOG("[Client] fd=" << _fd << " incomplete request, waiting for more data");
		}
		
		return true;
		
	} else if (bytes == 0) {
		DEBUG_LOG("[Client] fd=" << _fd << " closed connection gracefully");

		// 미완성 요청이 남아있는지 체크 (Content-Length 불일치 등)
		if (!_raw_buffer.empty() && _request && _request->getLastError() == HttpRequest::PARSE_INCOMPLETE) {
			ERROR_LOG("[Client] fd=" << _fd << " connection closed with incomplete request (Content-Length mismatch)");
			// 400 Bad Request 응답 생성
			_response = new HttpResponse(HttpResponse::createErrorResponse(StatusCode::BAD_REQUEST, NULL, NULL)); // 체크
			setState(WRITING_RESPONSE);
			return true;  // 응답 전송 시도 (클라이언트가 이미 종료했을 수 있음)
		}

		setState(DISCONNECTED);
		return false;
		
	} else {
		// bytes < 0: 논블로킹 소켓에서는 EAGAIN/EWOULDBLOCK 확인 필수
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			DEEP_LOG("[Client] fd=" << _fd << " read would block");
			return true;
		}
		ERROR_LOG("[Client] fd=" << _fd << " read error: " << std::strerror(errno));
		setState(DISCONNECTED);
		return false;
	}
}


bool Client::handleWrite() {
	if (_state != WRITING_RESPONSE || !_response) {
		return true;
	}
	
	// HttpResponse에서 완전한 HTTP 응답 생성
	if (_response_buffer.empty()) {
		_response_buffer = _response->serialize(_request);
		DEEP_LOG("[Client] fd=" << _fd << " serialized response size=" << response_data.size());
	
		if (_response_buffer.empty()) {
			ERROR_LOG("[Client] fd=" << _fd << " CRITICAL: serialize() returned empty buffer!");
			setState(DISCONNECTED);
			return false;
		}
	}

	size_t remaining = _response_buffer.size() - _response_sent;
	ssize_t bytes = ::send(_fd, _response_buffer.c_str() + _response_sent, remaining, 0);
	
	updateActivity();
	
	if (bytes > 0) {
		_response_sent += bytes;
		DEEP_LOG("[Client] fd=" << _fd << " sent " << bytes << " bytes, total=" << _response_sent << "/" << _response_buffer.size());
		
		if (_response_sent >= _response_buffer.size()) {
			DEBUG_LOG("[Client] fd=" << _fd << " response sent completely");
			
			// Keep-Alive 처리는 HttpRequest 객체를 통해
			if (_request && _request->isKeepAlive()) {
				DEEP_LOG("[Client] fd=" << _fd << " Keep-Alive enabled, resetting for next request");
				resetForNextRequest(); // 다음 요청을 위해 초기화
			} else {
				DEBUG_LOG("[Client] fd=" << _fd << " closing connection");
				setState(DISCONNECTED);
				return false; // 연결 종료
			}
		}
		
		return true;
		
	} else if (bytes == 0) {
		DEBUG_LOG("[Client] fd=" << _fd << " write returned 0");
		setState(DISCONNECTED);
		return false;
		
	} else {
		// bytes < 0: 에러 발생
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			DEEP_LOG("[Client] fd=" << _fd << " write would block");
			return true;
		}
		ERROR_LOG("[Client] fd=" << _fd << " write error: " << std::strerror(errno));
		setState(DISCONNECTED);
		return false;
	}
}


void Client::setResponse(HttpResponse* response) {
	DEBUG_LOG("[Client] fd=" << _fd << " setting new response");
	delete _response;
	_response = response;
	_response_sent = 0;
	setState(WRITING_RESPONSE);
}


bool Client::needsWriteEvent() const {
	return _state == WRITING_RESPONSE && _response != NULL;
}


void Client::resetForNextRequest() {
	DEBUG_LOG("[Client] fd=" << _fd << " resetting for next request");
	
	// 메모리 해제
	delete _response;
	_response = NULL;
	
	// HttpRequest는 재사용 (reset() 메서드가 있다고 가정)
	if (_request) {
		_request->reset();
	} else {
		_request = new HttpRequest();
	}
	
	// 버퍼 초기화
	_raw_buffer.clear();
	_response_buffer.clear();
	_response_sent = 0;
	
	setState(READING_REQUEST);
}
