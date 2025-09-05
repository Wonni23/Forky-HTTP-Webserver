#include "HttpResponse.hpp"

// 생성자와 소멸자
HttpResponse::HttpResponse() : _statusCode(200), _statusMessage("OK") {}

HttpResponse::~HttpResponse() {}

// Setter 함수들
void HttpResponse::setHeader(const std::string& key, const std::string& value) {
    _headers[key] = value;
}

void HttpResponse::setBody(const std::string& body) {
    _body = body;
}

std::string HttpResponse::toString() const {
    std::stringstream ss;
    
    // Status Line
    ss << "HTTP/1.1 " << _statusCode << " " << _statusMessage << "\r\n";
    
    // Headers
    for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
         it != _headers.end(); ++it) {
        ss << it->first << ": " << it->second << "\r\n";
    }
    
    // 빈 줄
    ss << "\r\n";
    
    // Body
    ss << _body;
    
    return ss.str();
}

// 에러 응답 생성 (과제 요구사항: 기본 에러 페이지)
HttpResponse HttpResponse::createErrorResponse(int code, const std::string& message) {
    HttpResponse response;
    response.setStatus(code, message);
    
    // HTML 에러 페이지 생성
    std::stringstream body;
    body << "<!DOCTYPE html>\n";
    body << "<html>\n";
    body << "<head>\n";
    body << "    <title>" << code << " " << message << "</title>\n";
    body << "    <style>\n";
    body << "        body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }\n";
    body << "        h1 { color: #d32f2f; }\n";
    body << "    </style>\n";
    body << "</head>\n";
    body << "<body>\n";
    body << "    <h1>" << code << " " << message << "</h1>\n";
    body << "    <p>webserv/1.0</p>\n";
    body << "</body>\n";
    body << "</html>\n";
    
    response.setBody(body.str());
    response.setContentType("text/html");
    response.setContentLength(response.getBody().length());
    response.setServerHeader();
    
    return response;
}

// 서버 헤더 설정
void HttpResponse::setServerHeader() {
    setHeader("Server", "webserv/1.0");
}

// 상태 코드 유효성 검사
bool HttpResponse::isValidStatusCode(int code) const {
    return (code >= 100 && code < 600);
}

void HttpResponse::setStatus(int code, const std::string& message) {
    if (!isValidStatusCode(code)) {
        _statusCode = 500;
        _statusMessage = "Internal Server Error";
        return;
    }
    
    _statusCode = code;
    _statusMessage = message;
}

void HttpResponse::setContentType(const std::string& type) {
    setHeader("Content-Type", type);
}

void HttpResponse::setContentLength(size_t length) {
    std::stringstream ss;
    ss << length;
    setHeader("Content-Length", ss.str());
}