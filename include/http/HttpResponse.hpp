#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <ctime>

class HttpResponse {
private:
    int _statusCode;              // 200, 404, 500 등
    std::string _statusMessage;   // OK, Not Found 등
    std::map<std::string, std::string> _headers;
    std::string _body;
    
    // Helper 함수들
    static std::string getStatusMessage(int code);
    static std::string createFallbackErrorPage(int code, const std::string& message);
	static std::string getErrorDescription(int code);
    
public:
    HttpResponse();
    ~HttpResponse();
    
    // ============ Setter 함수들 ============
    void setStatus(int code, const std::string& message);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);
    
    // ============ Getter 함수들 ============
    int getStatusCode() const { return _statusCode; }
    const std::string& getStatusMessage() const { return _statusMessage; }
    const std::string& getBody() const { return _body; }
    const std::string& getHeader(const std::string& key) const;
    
    // ============ 응답 생성 ============
    std::string toString() const;
    
    // ============ 에러 응답 생성 ============
    static HttpResponse createErrorResponse(int code);
    
    // ============ 편의 함수들 ============
    void setContentType(const std::string& type);
    void setContentLength(size_t length);
    void setServerHeader();
    void setDateHeader();
    void setConnectionHeader(const std::string& type = "close");
    void setDefaultHeaders();
    
    // ============ 유틸리티 ============
    bool isValidStatusCode(int code) const;
};

#endif // HTTP_RESPONSE_HPP