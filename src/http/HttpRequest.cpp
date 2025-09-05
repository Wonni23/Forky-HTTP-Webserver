#include "HttpRequest.hpp"

// 생성자와 소멸자
HttpRequest::HttpRequest() : _isComplete(false) {}

HttpRequest::~HttpRequest() {}

// Getter 함수들
const std::string& HttpRequest::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(toLowerCase(key));
    if (it != _headers.end()) {
        return it->second;
    }
    static const std::string empty = "";
    return empty;
}

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

bool HttpRequest::parseRequest(const std::string& rawData) {
    // 1. CRLF CRLF로 헤더와 바디 분리
    size_t headerEnd = rawData.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return false; // 아직 완전한 헤더가 오지 않음
    }
    
    std::string headerPart = rawData.substr(0, headerEnd);
    std::string bodyPart = rawData.substr(headerEnd + 4);
    
    // 2. 헤더 파싱
    if (!parseHeaders(headerPart)) {
        return false;
    }
    
    // 3. 청크 인코딩 확인 (과제 필수 요구사항)
    if (isChunkedEncoding()) {
        return parseChunkedBody(bodyPart);
    }
    
    // 4. Content-Length 기반 바디 처리
    size_t expectedLength = getContentLength();
    if (expectedLength == 0) {
        // GET 요청 등에서는 바디가 없을 수 있음
        _body = "";
        _isComplete = true;
        return true;
    }
    
    if (bodyPart.length() >= expectedLength) {
        _body = bodyPart.substr(0, expectedLength);
        _isComplete = true;
        return true;
    }
    
    return false; // 바디가 아직 완전히 오지 않음
}

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

// Transfer-Encoding: chunked인 경우
bool HttpRequest::isChunkedEncoding() const {
    std::string encoding = getHeader("transfer-encoding");
    return toLowerCase(encoding) == "chunked";
}

// 유틸리티 함수들
std::string HttpRequest::trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

std::string HttpRequest::toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

// 과제에서 명시적으로 요구하는 청크 인코딩 처리
bool HttpRequest::parseChunkedBody(const std::string& data) {
    _body.clear();
    size_t pos = 0;
    
    while (pos < data.length()) {
        // 청크 크기 줄 찾기
        size_t crlfPos = data.find("\r\n", pos);
        if (crlfPos == std::string::npos) {
            return false; // 아직 완전하지 않음
        }
        
        // 청크 크기 파싱 (16진수)
        std::string sizeStr = data.substr(pos, crlfPos - pos);
        char* endPtr;
        size_t chunkSize = std::strtoul(sizeStr.c_str(), &endPtr, 16);
        
        if (*endPtr != '\0') {
            return false; // 잘못된 청크 크기
        }
        
        pos = crlfPos + 2; // CRLF 건너뛰기
        
        // 마지막 청크 (크기 0)
        if (chunkSize == 0) {
            // 트레일러 헤더들 건너뛰기 (과제에서는 무시)
            size_t finalCrlf = data.find("\r\n\r\n", pos);
            if (finalCrlf == std::string::npos) {
                return false; // 아직 완전하지 않음
            }
            _isComplete = true;
            return true;
        }
        
        // 청크 데이터 확인
        if (pos + chunkSize + 2 > data.length()) {
            return false; // 아직 완전하지 않음
        }
        
        // 청크 데이터 추가
        _body += data.substr(pos, chunkSize);
        pos += chunkSize + 2; // 데이터 + CRLF 건너뛰기
    }
    
    return false; // 아직 완료되지 않음
}

// URL 디코딩 (브라우저 호환성을 위해)
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