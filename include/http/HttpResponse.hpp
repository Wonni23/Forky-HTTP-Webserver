#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <string>
#include <map>
#include <iostream>
#include <sstream>

class HttpResponse {
private:
	int _statusCode;              // 200, 404, 500 등
	std::string _statusMessage;   // OK, Not Found 등
	std::map<std::string, std::string> _headers;
	std::string _body;
	
public:
	HttpResponse();
	~HttpResponse();
	
	// Setter 함수들
	void setStatus(int code, const std::string& message);
	void setHeader(const std::string& key, const std::string& value);
	void setBody(const std::string& body);
	
	// 응답 생성
	std::string toString() const;

	// 에러 응답 생성 (과제에서 기본 에러 페이지 요구)
    static HttpResponse createErrorResponse(int code, const std::string& message);
	
	// 편의 함수들
	void setContentType(const std::string& type);
	void setContentLength(size_t length);

	// 에러 응답 생성 관련 편의 함수들
	void setServerHeader();
	bool isValidStatusCode(int code) const;
};

#endif // HTTP_RESPONSE_HPP