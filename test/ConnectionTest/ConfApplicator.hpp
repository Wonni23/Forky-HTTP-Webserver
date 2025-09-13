#ifndef CONF_APPLICATOR_HPP
#define CONF_APPLICATOR_HPP

#include "webserv.hpp"
#include "ConfigDTO.hpp"
#include "Server.hpp"
// #include "HttpRequest.hpp"

class ConfApplicator {
private:
	// 전역으로 관리될 최종 설정 객체
	static ConfigDTO*	_global_config;

public:
	ConfApplicator();
	~ConfApplicator();

	// 파싱된 설정을 Server 객체에 적용하는 메인 함수
	bool				applyConfig(Server* server, const ConfigDTO& config);

	// 프로그램 전역에서 설정에 접근하기 위한 static 함수들
	static void			setGlobalConfig(const ConfigDTO& config);
	static ConfigDTO*	getGlobalConfig();

	// 특정 HTTP 요청에 가장 적합한 Server/Location Context를 찾는 함수들
	// static const ServerContext*		findServerForRequest(const HttpRequest* request, int connected_port);
	// static const LocationContext*	findLocationForRequest(const ServerContext* server, const std::string& uri);
};

#endif
