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

	bool parseHeaders(const std::string& headerPart);
    bool parseRequestLine(const std::string& line);
    bool parseChunkedBody(const std::string& data);
    std::string urlDecode(const std::string& str) const;

public:
	HttpRequest();
	~HttpRequest();
	
	// 파싱 관련
	bool parseRequest(const std::string& rawData);
	bool isComplete() const { return _isComplete; }
	
	// Getter 함수들
	const std::string& getMethod() const { return _method; }
	const std::string& getUri() const { return _uri; }
	const std::string& getVersion() const { return _version; }
	const std::string& getHeader(const std::string& key) const;
	const std::string& getBody() const { return _body; }
	
	// 유틸리티
	bool hasHeader(const std::string& key) const;
	size_t getContentLength() const;
	bool isChunkedEncoding() const;
	
	// 내부 유틸리티 함수들
	std::string trim(const std::string& str) const;
	std::string toLowerCase(const std::string& str) const;
};

#endif // HTTP_REQUEST_HPP