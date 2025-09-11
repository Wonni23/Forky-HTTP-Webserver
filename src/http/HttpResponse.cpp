#include "HttpResponse.hpp"

// ============ 생성자와 소멸자 ============
HttpResponse::HttpResponse() : _statusCode(200), _statusMessage("OK") {}

HttpResponse::~HttpResponse() {}

// ============ Setter 함수들 ============
void HttpResponse::setStatus(int code, const std::string& message) {
    if (!isValidStatusCode(code)) {
        _statusCode = 500;
        _statusMessage = "Internal Server Error";
        return;
    }
    
    _statusCode = code;
    _statusMessage = message;
}

void HttpResponse::setHeader(const std::string& key, const std::string& value) {
    _headers[key] = value;
}

void HttpResponse::setBody(const std::string& body) {
    _body = body;
}

// ============ Getter 함수들 ============
const std::string& HttpResponse::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    if (it != _headers.end()) {
        return it->second;
    }
    static const std::string empty = "";
    return empty;
}

// ============ 응답 생성 ============
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

std::string HttpResponse::loadErrorPageFile(const std::string& errorPagePath) {
    std::ifstream file(errorPagePath.c_str());
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    return ""; // 파일 없음 -> 하드코딩 fallback
}

HttpResponse HttpResponse::createErrorResponse(int code, const std::string& errorPagePath) {
    HttpResponse response;
    std::string message = getStatusMessage(code);
    response.setStatus(code, message);
    
    std::string errorPage;
    
    // 1. 설정된 에러 파일 시도
    if (!errorPagePath.empty()) {
        errorPage = loadErrorPageFile(errorPagePath);
    }
    
    // 2. 실패시 하드코딩 에러페이지 fallback
    if (errorPage.empty()) {
        errorPage = createFallbackErrorPage(code, message);
    }
    
    response.setBody(errorPage);
    response.setContentType("text/html");
    response.setContentLength(response.getBody().length());
    response.setDefaultHeaders();
    
    return response;
}

// ============ 편의 함수들 ============
void HttpResponse::setContentType(const std::string& type) {
    setHeader("Content-Type", type);
}

void HttpResponse::setContentLength(size_t length) {
    std::stringstream ss;
    ss << length;
    setHeader("Content-Length", ss.str());
}

void HttpResponse::setServerHeader() {
    setHeader("Server", "webserv/1.0");
}

void HttpResponse::setDateHeader() {
    // RFC 1123 형식의 날짜 생성
    time_t now;
    time(&now);
    struct tm* gmt = gmtime(&now);
    
    if (gmt) {
        char date_str[128];
        strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", gmt);
        setHeader("Date", date_str);
    }
}

void HttpResponse::setConnectionHeader(const std::string& type) {
    setHeader("Connection", type);
}

void HttpResponse::setDefaultHeaders() {
    setServerHeader();
    setDateHeader();
    setConnectionHeader();
}

// ============ 유틸리티 ============
bool HttpResponse::isValidStatusCode(int code) const {
    return (code >= 100 && code < 600);
}

// ============ Helper 함수들 ============
std::string HttpResponse::getStatusMessage(int code) {
    switch (code) {
        // 2xx Success
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        
        // 3xx Redirection  
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        
        // 4xx Client Error
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 415: return "Unsupported Media Type";
        
        // 5xx Server Error
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        
        default: return "Unknown Error";
    }
}

std::string HttpResponse::createFallbackErrorPage(int code, const std::string& message) {
    std::stringstream html;
    html << "<!DOCTYPE html>\n";
    html << "<html>\n";
    html << "<head>\n";
    html << "    <title>" << code << " " << message << "</title>\n";
    html << "    <meta charset=\"UTF-8\">\n";
    html << "    <style>\n";
    html << "        body {\n";
    html << "            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;\n";
    html << "            text-align: center;\n";
    html << "            margin: 0;\n";
    html << "            padding: 50px 20px;\n";
    html << "            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\n";
    html << "            color: white;\n";
    html << "            min-height: 100vh;\n";
    html << "            display: flex;\n";
    html << "            flex-direction: column;\n";
    html << "            justify-content: center;\n";
    html << "        }\n";
    html << "        .container {\n";
    html << "            max-width: 600px;\n";
    html << "            margin: 0 auto;\n";
    html << "            background: rgba(255, 255, 255, 0.1);\n";
    html << "            padding: 40px;\n";
    html << "            border-radius: 15px;\n";
    html << "            backdrop-filter: blur(10px);\n";
    html << "            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);\n";
    html << "        }\n";
    html << "        h1 {\n";
    html << "            font-size: 4em;\n";
    html << "            margin: 0 0 20px 0;\n";
    html << "            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);\n";
    html << "        }\n";
    html << "        h2 {\n";
    html << "            font-size: 1.5em;\n";
    html << "            margin: 0 0 20px 0;\n";
    html << "            font-weight: 300;\n";
    html << "        }\n";
    html << "        p {\n";
    html << "            font-size: 1.1em;\n";
    html << "            margin: 20px 0;\n";
    html << "            opacity: 0.9;\n";
    html << "            line-height: 1.6;\n";
    html << "        }\n";
    html << "        .footer {\n";
    html << "            margin-top: 30px;\n";
    html << "            font-size: 0.9em;\n";
    html << "            opacity: 0.7;\n";
    html << "            border-top: 1px solid rgba(255,255,255,0.2);\n";
    html << "            padding-top: 20px;\n";
    html << "        }\n";
    html << "    </style>\n";
    html << "</head>\n";
    html << "<body>\n";
    html << "    <div class=\"container\">\n";
    html << "        <h1>" << code << "</h1>\n";
    html << "        <h2>" << message << "</h2>\n";
    html << "        <p>" << getErrorDescription(code) << "</p>\n";
    html << "        <div class=\"footer\">webserv/1.0</div>\n";
    html << "    </div>\n";
    html << "</body>\n";
    html << "</html>\n";
    
    return html.str();
}

// Helper 함수: 에러 설명 추가
std::string HttpResponse::getErrorDescription(int code) {
    switch (code) {
        case 400: return "The server could not understand the request due to invalid syntax.";
        case 401: return "Authentication is required to access this resource.";
        case 403: return "You don't have permission to access this resource.";
        case 404: return "The requested resource could not be found on this server.";
        case 405: return "The request method is not allowed for this resource.";
        case 408: return "The server timed out waiting for the request.";
        case 413: return "The request payload is too large.";
        case 414: return "The requested URI is too long for the server to process.";
        case 415: return "The media type of the request is not supported by the server.";

        case 500: return "The server encountered an unexpected condition.";
        case 501: return "The server does not support the functionality required.";
        case 502: return "The server received an invalid response from the upstream server.";
        case 503: return "The server is temporarily unavailable.";
        case 504: return "The server did not receive a timely response from the upstream server.";
        case 505: return "The HTTP version used in the request is not supported by the server.";

        default: return "An error occurred while processing your request.";
    }
}