#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

class HttpRequest {
private:
    std::string _method;          // GET, POST, DELETE
    std::string _uri;             // /index.html
    std::string _version;         // HTTP/1.1
    std::map<std::string, std::string> _headers;
    std::string _body;
    bool _isComplete;             // 파싱 완료 여부

    // 파싱 관련 private 함수들
    bool parseHeaders(const std::string& headerPart);
    bool parseRequestLine(const std::string& line);
    std::string urlDecode(const std::string& str) const;

public:
    HttpRequest();
    ~HttpRequest();
    
    // 완성된 HTTP 요청 파싱 (Client에서 모든 처리 완료 후 호출)
    bool parseRequest(const std::string& completeHttpRequest);
    
    // 헤더만 파싱 (Client에서 청크 여부 확인용)
    bool parseHeadersOnly(const std::string& headerPart);
    
    // ============ Getter 함수들 ============
    const std::string& getMethod() const { return _method; }
    const std::string& getUri() const { return _uri; }
    const std::string& getVersion() const { return _version; }
    const std::string& getHeader(const std::string& key) const;
    const std::string& getBody() const { return _body; }
    
    // ============ 상태 확인 ============
    bool isComplete() const { return _isComplete; }
    
    // ============ 유틸리티 ============
    bool hasHeader(const std::string& key) const;
    size_t getContentLength() const;
    bool isChunkedEncoding() const;
    
    // 재사용을 위한 초기화
    void reset();
    
    // 내부 유틸리티 함수들
    std::string trim(const std::string& str) const;
    std::string toLowerCase(const std::string& str) const;
};

#endif // HTTP_REQUEST_HPP