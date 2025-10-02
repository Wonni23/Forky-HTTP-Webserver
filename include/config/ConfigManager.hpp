#ifndef CONFIGMANAGER_HPP
#define CONFIGMANAGER_HPP

#include "webserv.hpp"
#include "dto/ConfigDTO.hpp"
#include "server/Server.hpp"
#include "http/HttpRequest.hpp"

class ConfigManager {
private:
	// 전역으로 관리될 최종 설정 객체
	static ConfigDTO*	_global_config;

public:
	ConfigManager();
	~ConfigManager();

	// 파싱된 설정을 Server 객체에 적용하는 메인 함수
	bool				applyConfig(Server* server, const ConfigDTO& config);

	// 프로그램 전역에서 설정에 접근하기 위한 static 함수들
	static void			setGlobalConfig(const ConfigDTO& config);
	static ConfigDTO*	getGlobalConfig();
};
// Would be better if this class is refactored in Singleton pattern


#endif
