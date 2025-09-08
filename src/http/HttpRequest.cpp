#include "HttpRequest.hpp"

// ============ Private 파싱 함수들 ============

bool HttpRequest::parseHeaders(const std::string& headerPart) {
    std::istringstream iss(headerPart);
    std::string line;
    
    // 첫 번째 줄: Request Line 파싱
    if (!std::getline(iss, line)) return false;
    
    // \r 제거
    if (!line.empty() && line[line.length()-1] == '\r') {
        line.erase(line.length()-1);
    }
    
    if (!parseRequestLine(line)) return false;
    
    // 나머지 줄들: 헤더 파싱
    while (std::getline(iss, line)) {
        // \r 제거
        if (!line.empty() && line[line.length()-1] == '\r') {
            line.erase(line.length()-1);
        }
        if (line.empty()) break;
        
        // "Key: Value" 형태 파싱
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;
        
        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);
        
        // 공백 제거
        key = trim(key);
        value = trim(value);
        
        // 헤더 키는 대소문자 구분 없음
        _headers[toLowerCase(key)] = value;
    }
    
    return true;
}

bool HttpRequest::parseRequestLine(const std::string& line) {
    std::istringstream iss(line);
    
    // "GET /index.html HTTP/1.1" 파싱
    if (!(iss >> _method >> _uri >> _version)) {
        return false;
    }
    
    // URL 디코딩 적용
    _uri = urlDecode(_uri);
    
    // 메서드 검증
    if (_method != "GET" && _method != "POST" && _method != "DELETE") {
        return false;
    }
    
    // 버전 검증
    if (_version != "HTTP/1.1" && _version != "HTTP/1.0") {
        return false;
    }
    
    return true;
}

std::string HttpRequest::urlDecode(const std::string& str) const {
    std::string result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            // %XX 형태를 ASCII 문자로 변환
            char hex[3] = {str[i+1], str[i+2], '\0'};
            char* endPtr;
            int ascii = std::strtol(hex, &endPtr, 16);
            
            if (*endPtr == '\0' && ascii >= 0 && ascii <= 255) {
                result += static_cast<char>(ascii);
                i += 2;
            } else {
                result += str[i]; // 잘못된 인코딩은 그대로 유지
            }
        } else if (str[i] == '+') {
            result += ' '; // + 는 공백으로 변환
        } else {
            result += str[i];
        }
    }
    
    return result;
}

// 생성자와 소멸자
HttpRequest::HttpRequest() : _isComplete(false) {}

HttpRequest::~HttpRequest() {}

// 완성된 HTTP 요청 파싱 (청크 처리는 이미 Client에서 완료됨)
bool HttpRequest::parseRequest(const std::string& completeHttpRequest) {
    reset(); // 초기화
    
    // 1. 헤더와 바디 분리
    size_t headerEnd = completeHttpRequest.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return false; // 잘못된 형식
    }
    
    std::string headerPart = completeHttpRequest.substr(0, headerEnd);
    std::string bodyPart = completeHttpRequest.substr(headerEnd + 4);
    
    // 2. 헤더 파싱
    if (!parseHeaders(headerPart)) {
        return false;
    }
    
    // 3. 바디 설정 (Client에서 이미 청크 처리 완료)
    _body = bodyPart;
    _isComplete = true;
    
    return true;
}

// 헤더만 파싱 (Client에서 청크 여부 확인용)
bool HttpRequest::parseHeadersOnly(const std::string& headerPart) {
    return parseHeaders(headerPart);
}

// ============ Getter 함수들 ============

const std::string& HttpRequest::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(toLowerCase(key));
    if (it != _headers.end()) {
        return it->second;
    }
    static const std::string empty = "";
    return empty;
}

// ============ 유틸리티 함수들 ============

bool HttpRequest::hasHeader(const std::string& key) const {
    return _headers.find(toLowerCase(key)) != _headers.end();
}

size_t HttpRequest::getContentLength() const {
    if (hasHeader("content-length")) {
        const std::string& lengthStr = getHeader("content-length");
        return static_cast<size_t>(std::atoi(lengthStr.c_str()));
    }
    return 0;
}

bool HttpRequest::isChunkedEncoding() const {
    std::string encoding = getHeader("transfer-encoding");
    return toLowerCase(encoding) == "chunked";
}

void HttpRequest::reset() {
    _method.clear();
    _uri.clear();
    _version.clear();
    _headers.clear();
    _body.clear();
    _isComplete = false;
}

std::string HttpRequest::trim(const std::string& str) const {
    const std::string whitespace = " \t\r\n";
    
    size_t first = str.find_first_not_of(whitespace);
    if (first == std::string::npos) return "";
    
    size_t last = str.find_last_not_of(whitespace);
    return str.substr(first, (last - first + 1));
}

std::string HttpRequest::toLowerCase(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}