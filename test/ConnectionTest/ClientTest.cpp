#include "ClientTest.hpp"
// HTTP 헤더들 주석 처리
// #include "http/HttpRequest.hpp"
// #include "http/HttpResponse.hpp"
// #include "http/StatusCode.hpp"

Client::Client(int fd) 
    : _fd(fd), _state(READING_REQUEST), _echo_data(""), 
      _response_sent(0), _last_activity(0) {
    updateActivity();
    DEBUG_LOG("Client created for fd=" << _fd);
}


Client::~Client() {
    // delete _current_request;  // HTTP 객체 사용 안 함
    // delete _current_response; // HTTP 객체 사용 안 함
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
            setState(DISCONNECTED);
            return false;
        }
        
        // ✅ HTTP 파싱 대신: 개행 문자가 오면 에코 준비 완료
        if (_raw_buffer.find('\n') != std::string::npos || 
            _raw_buffer.find('\r') != std::string::npos) {
            DEBUG_LOG("Echo data ready from fd=" << _fd << ": " << _raw_buffer);
            setState(PROCESSING_REQUEST);
        }
        
        return true;
        
    } else if (bytes == 0) {
        DEBUG_LOG("Client fd=" << _fd << " closed connection");
        setState(DISCONNECTED);
        return false;
        
    } else {
        DEBUG_LOG("Read error from fd=" << _fd);
        setState(DISCONNECTED);
        return false;
    }
}

bool Client::handleWrite() {
    if (_state != WRITING_RESPONSE) {
        return true;
    }
    
    // ✅ HTTP Response 대신: 받은 데이터를 그대로 전송
    size_t remaining = _echo_data.size() - _response_sent;
    ssize_t bytes = ::send(_fd, _echo_data.c_str() + _response_sent, remaining, 0);
    
    updateActivity();
    
    if (bytes > 0) {
        _response_sent += bytes;
        
        if (_response_sent >= _echo_data.size()) {
            DEBUG_LOG("Echo sent completely to fd=" << _fd);
            
        // ✅ 수정: 다음 요청을 위해 초기화하고 대기 상태로 전환
            _echo_data.clear();
            _raw_buffer.clear();
            _response_sent = 0;
            setState(READING_REQUEST);
            
            return true; // ✅ 연결 유지
        }
        
        return true;
        
    } else if (bytes == 0) {
        DEBUG_LOG("Write returned 0 for fd=" << _fd);
        setState(DISCONNECTED);
        return false;
        
    } else {
        DEBUG_LOG("Write error for fd=" << _fd);
        setState(DISCONNECTED);
        return false;
    }
}

// ✅ HTTP Response 대신 에코 데이터 설정
void Client::setEchoData(const std::string& data) {
    _echo_data = "ECHO: " + data;  // 구분을 위해 ECHO: 접두사 추가
    _response_sent = 0;
    setState(WRITING_RESPONSE);
}

bool Client::needsWriteEvent() const {
    return _state == WRITING_RESPONSE;
}

// ✅ 에코용 getter 추가
std::string Client::getRawBuffer() const {
    return _raw_buffer;
}
