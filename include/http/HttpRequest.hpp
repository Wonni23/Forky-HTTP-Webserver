// include/http/HttpRequest.hpp
#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>

class HttpRequest {
private:
    std::string _method;
    std::string _uri;
    std::string _version;
    std::map<std::string, std::string> _headers;
    
    // 🔥 Zero-Copy Body 최적화
    std::string _body;                    // 기존: 복사된 body (chunked 디코딩용)
    const std::string* _bodyBufferRef;    // 🆕 버퍼 참조 (zero-copy)
    size_t _bodyStart;                    // 🆕 body 시작 위치
    size_t _bodyLength;                   // 🆕 body 길이
    
    size_t _contentLength;
    bool _isChunked;
    int _statusCodeForError;
    
public:
    HttpRequest();
    ~HttpRequest();
    
    // 헤더 파싱
    bool parseHeaders(const std::string& headerStr);
    
    // Body 관리 (기존 방식 - chunked용)
    void setDecodedBody(const std::string& body);
    const std::string& getBody() const;
    
    // 🆕 Zero-Copy Body 관리 (CGI용)
    void setBodyReference(const std::string* buffer, size_t start, size_t length);
    const char* getBodyData() const;
    size_t getBodyLength() const;
    bool isBodyByReference() const;
    
    // Chunked 디코딩
    std::string decodeChunkedBody(const std::string& rawBody) const;
    
    // Getter
    const std::string& getMethod() const;
    const std::string& getUri() const;
    const std::string& getVersion() const;
    std::string getHeader(const std::string& key) const;
    const std::map<std::string, std::string>& getHeaders() const;
    bool hasHeader(const std::string& key) const;
    
    size_t getContentLength() const;
    bool isChunkedEncoding() const;
    bool isKeepAlive() const;
    
    int getStatusCodeForError() const;
};

#endif
