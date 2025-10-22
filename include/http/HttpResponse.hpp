#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <string>
#include <map>
#include <vector>

#include "http/HttpRequest.hpp"
#include "dto/ConfigDTO.hpp"

class HttpResponse {
private:
	int									_statusCode;
	std::map<std::string, std::string>	_headers;
	std::string 						_body;

	// Internal Utility
	void	setDefaultHeaders(const HttpRequest* request);
	
public:
	HttpResponse();
	~HttpResponse();
	
	/* Setters */
	void setStatus(int code);
	void setHeader(const std::string& key, const std::string& value);
	void setBody(const std::string& body);
	void setContentType(const std::string& type);

	/* Cookie Management */
	void addCookie(const std::string& name, const std::string& value, int maxAge = -1, const std::string& path = "/", bool httpOnly = true);
	void deleteCookie(const std::string& name, const std::string& path = "/");
	

	/* 응답 생성 */
	std::string serialize(const HttpRequest* request);
	
	/* 에러 응답 생성 */
	static HttpResponse createErrorResponse(int code, const ServerContext* serverConf, const LocationContext* locConf);
};

#endif // HTTP_RESPONSE_HPP