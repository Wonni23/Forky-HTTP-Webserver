// src/http/HttpRequest.cpp
#include "http/HttpRequest.hpp"
#include "http/StatusCode.hpp"
#include "utils/StringUtils.hpp"
#include "utils/Common.hpp"
#include <sstream>
#include <algorithm>

// ========= 생성자 및 소멸자 =======
HttpRequest::HttpRequest()
    : _bodyBufferRef(NULL), _bodyStart(0), _bodyLength(0),
      _contentLength(0), _isChunked(false), _statusCodeForError(0)
{
}

HttpRequest::~HttpRequest()
{
}

// ========= 헤더 파싱 =======
bool HttpRequest::parseHeaders(const std::string& headerStr)
{
    std::istringstream stream(headerStr);
    std::string line;
    
    // 1. Request-Line 파싱 (GET /path HTTP/1.1)
    if (!std::getline(stream, line)) {
        _statusCodeForError = StatusCode::BAD_REQUEST;
        return false;
    }
    
    // \r 제거
    if (!line.empty() && line[line.length() - 1] == '\r') {
        line.erase(line.length() - 1);
    }
    
    // Method, URI, Version 추출
    std::istringstream requestLine(line);
    requestLine >> _method >> _uri >> _version;
    
    if (_method.empty() || _uri.empty() || _version.empty()) {
        _statusCodeForError = StatusCode::BAD_REQUEST;
        return false;
    }
    
    // Method 대문자 변환
    std::transform(_method.begin(), _method.end(), _method.begin(), ::toupper);
    
    // HTTP 버전 체크
    if (_version != "HTTP/1.1" && _version != "HTTP/1.0") {
        _statusCodeForError = StatusCode::HTTP_VERSION_NOT_SUPPORTED;
        return false;
    }
    
    DEBUG_LOG("[HttpRequest] Parsed request line: " 
        << _method << " " << _uri << " " << _version);
    
    // 2. 헤더 파싱
    while (std::getline(stream, line)) {
        // \r 제거
        if (!line.empty() && line[line.length() - 1] == '\r') {
            line.erase(line.length() - 1);
        }
        
        // 빈 줄이면 헤더 끝
        if (line.empty()) {
            break;
        }
        
        // "Key: Value" 형태 파싱
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            continue;  // 잘못된 헤더는 무시
        }
        
        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);
        
        // 앞뒤 공백 제거
        key = StringUtils::trim(key);
        value = StringUtils::trim(value);
        
        // 헤더 키를 소문자로 변환 (대소문자 구분 없음)
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        
        _headers[key] = value;
    }
    
    DEBUG_LOG("[HttpRequest] Parsed " << _headers.size() << " headers");
    
    // 3. Content-Length 파싱
    if (hasHeader("content-length")) {
        std::string contentLengthStr = getHeader("content-length");
        std::istringstream iss(contentLengthStr);
        iss >> _contentLength;
        
        if (iss.fail()) {
            _statusCodeForError = StatusCode::BAD_REQUEST;
            return false;
        }
        
        DEBUG_LOG("[HttpRequest] Content-Length: " << _contentLength);
    }
    
    // 4. Transfer-Encoding: chunked 확인
    if (hasHeader("transfer-encoding")) {
        std::string encoding = getHeader("transfer-encoding");
        std::transform(encoding.begin(), encoding.end(), encoding.begin(), ::tolower);
        
        if (encoding.find("chunked") != std::string::npos) {
            _isChunked = true;
            DEBUG_LOG("[HttpRequest] Transfer-Encoding: chunked");
        }
    }
    
    return true;
}

// ========= Body 관리 (기존 방식 - Chunked용) =======
void HttpRequest::setDecodedBody(const std::string& body)
{
    _body = body;
    _bodyBufferRef = NULL;  // 복사 방식으로 전환
    DEBUG_LOG("[HttpRequest] Body set (copied): " << body.length() << " bytes");
}

const std::string& HttpRequest::getBody() const
{
    return _body;
}

// ========= 🔥 Zero-Copy Body 관리 (CGI용) =======
void HttpRequest::setBodyReference(const std::string* buffer, size_t start, size_t length)
{
    _bodyBufferRef = buffer;
    _bodyStart = start;
    _bodyLength = length;
    DEBUG_LOG("[HttpRequest] Body reference set (zero-copy): " << length << " bytes");
}

const char* HttpRequest::getBodyData() const
{
    if (_bodyBufferRef) {
        return _bodyBufferRef->c_str() + _bodyStart;
    }
    return _body.c_str();
}

size_t HttpRequest::getBodyLength() const
{
    if (_bodyBufferRef) {
        return _bodyLength;
    }
    return _body.length();
}

bool HttpRequest::isBodyByReference() const
{
    return _bodyBufferRef != NULL;
}

// ========= Chunked 디코딩 =======
std::string HttpRequest::decodeChunkedBody(const std::string& rawBody) const
{
    std::string decoded;
    size_t pos = 0;
    
    while (pos < rawBody.length()) {
        // 1. 청크 크기 읽기 (16진수)
        size_t crlfPos = rawBody.find("\r\n", pos);
        if (crlfPos == std::string::npos) {
            ERROR_LOG("[HttpRequest] Invalid chunked encoding: no CRLF after chunk size");
            return "";
        }
        
        std::string chunkSizeStr = rawBody.substr(pos, crlfPos - pos);
        
        // 세미콜론 이후는 chunk-extension (무시)
        size_t semicolonPos = chunkSizeStr.find(';');
        if (semicolonPos != std::string::npos) {
            chunkSizeStr = chunkSizeStr.substr(0, semicolonPos);
        }
        
        // 16진수 → 10진수 변환
        size_t chunkSize;
        std::istringstream iss(chunkSizeStr);
        iss >> std::hex >> chunkSize;
        
        if (iss.fail()) {
            ERROR_LOG("[HttpRequest] Invalid chunk size: " << chunkSizeStr);
            return "";
        }
        
        // 2. 마지막 청크 (크기 0)
        if (chunkSize == 0) {
            DEBUG_LOG("[HttpRequest] Last chunk received, total decoded: " << decoded.length() << " bytes");
            break;
        }
        
        // 3. 청크 데이터 읽기
        pos = crlfPos + 2;  // "\r\n" 건너뛰기
        
        if (pos + chunkSize > rawBody.length()) {
            ERROR_LOG("[HttpRequest] Incomplete chunk data");
            return "";
        }
        
        decoded.append(rawBody.substr(pos, chunkSize));
        pos += chunkSize;
        
        // 4. 청크 끝의 CRLF 건너뛰기
        if (pos + 2 > rawBody.length() || rawBody.substr(pos, 2) != "\r\n") {
            ERROR_LOG("[HttpRequest] Missing CRLF after chunk data");
            return "";
        }
        
        pos += 2;
    }
    
    return decoded;
}

// ========= Getter =======
const std::string& HttpRequest::getMethod() const
{
    return _method;
}

const std::string& HttpRequest::getUri() const
{
    return _uri;
}

const std::string& HttpRequest::getVersion() const
{
    return _version;
}

std::string HttpRequest::getHeader(const std::string& key) const
{
    std::string lowerKey = key;
    std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
    
    std::map<std::string, std::string>::const_iterator it = _headers.find(lowerKey);
    if (it != _headers.end()) {
        return it->second;
    }
    return "";
}

const std::map<std::string, std::string>& HttpRequest::getHeaders() const
{
    return _headers;
}

bool HttpRequest::hasHeader(const std::string& key) const
{
    std::string lowerKey = key;
    std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
    
    return _headers.find(lowerKey) != _headers.end();
}

size_t HttpRequest::getContentLength() const
{
    return _contentLength;
}

bool HttpRequest::isChunkedEncoding() const
{
    return _isChunked;
}

bool HttpRequest::isKeepAlive() const
{
    // HTTP/1.1은 기본적으로 keep-alive
    if (_version == "HTTP/1.1") {
        std::string conn = getHeader("connection");
        std::transform(conn.begin(), conn.end(), conn.begin(), ::tolower);
        return conn != "close";
    }
    
    // HTTP/1.0은 명시적으로 Connection: keep-alive 필요
    if (_version == "HTTP/1.0") {
        std::string conn = getHeader("connection");
        std::transform(conn.begin(), conn.end(), conn.begin(), ::tolower);
        return conn == "keep-alive";
    }
    
    return false;
}

int HttpRequest::getStatusCodeForError() const
{
    return _statusCodeForError;
}
