#ifndef REQUESTROUTER_HPP
# define REQUESTROUTER_HPP

#include "webserv.hpp"
#include "dto/ConfigDTO.hpp"
#include "server/Server.hpp"
#include "http/HttpRequest.hpp"

class RequestRouter {
public:	
	// 특정 HTTP 요청에 가장 적합한 Server/Location Context를 찾는 함수들
	static const ServerContext*		findServerForRequest(const HttpRequest* request, int connected_port);
	static const LocationContext*	findLocationForRequest(const ServerContext* server, const std::string& uri, const std::string& method);
	static bool 					isMethodAllowedInLocation(const std::string& method, const LocationContext& loc);
};

#endif