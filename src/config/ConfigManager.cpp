#include "config/ConfigManager.hpp"
#include "http/HttpRequest.hpp"
#include "server/Server.hpp"
#include <sstream>

ConfigDTO* ConfigManager::_global_config = 0;

ConfigManager::ConfigManager() {}
ConfigManager::~ConfigManager() {}

bool ConfigManager::applyConfig(Server* server, const ConfigDTO& config) {
	// 1. Cascading이 완료된 최종 설정을 전역으로 저장.
	ConfigManager::setGlobalConfig(config);

	if (ConfigManager::getGlobalConfig() == 0) {
		ERROR_LOG("Failed to set global configuration");
		return false;
	}

	std::vector<ServerContext>& servers = ConfigManager::getGlobalConfig()->httpContext.serverContexts;

	// 2. 각 server 블록의 listen 지시어를 Server 객체에 등록.
	for (size_t i = 0; i < servers.size(); ++i) {
		ServerContext& serverCtx = servers[i];

		if (serverCtx.opListenDirective.empty()) {
			ERROR_LOG("Server block " << (i + 1) << " has no listen directive. Skipping.");
			continue;
		}

		const ListenDirective& listen = serverCtx.opListenDirective[0];

		if (listen.port == 0) {
			ERROR_LOG("Invalid port in listen directive: " << listen.address);
			continue;
		}

		if (!server->addListenPort(listen.host, listen.port)) {
			ERROR_LOG("Failed to bind to " << listen.host << ":" << listen.port);
			return false; // 포트 바인딩 실패
		}
	}
	return true;
}

void ConfigManager::setGlobalConfig(const ConfigDTO& config) {
	if (_global_config != 0) {
		delete _global_config;
	}
	_global_config = new ConfigDTO(config);
}

ConfigDTO* ConfigManager::getGlobalConfig() {
	return _global_config;
}

std::string ConfigManager::lookupErrorPage(int code, const std::vector<ErrorPageDirective>& directives) {
	// directives 벡터를 순회.
	for (std::vector<ErrorPageDirective>::const_iterator it = directives.begin(); it != directives.end(); ++it) {
		// 각 directive의 errorPageMap에서 code를 키로 가지는 원소를 찾음.
		std::map<int, std::string>::const_iterator mit = it->errorPageMap.find(code);
		if (mit != it->errorPageMap.end()) {
			return mit->second; // 경로 발견.
		}
	}
	return ""; // 경로 없음.
}

std::string ConfigManager::findErrorPagePath(int code, const ServerContext* serverCtx, const LocationContext* locCtx) {
	std::string path;

	if (serverCtx == NULL) {
		return "";  // 빈 문자열 반환
	} // 10.13

	// 1. Location 컨텍스트에서 탐색.
	if (locCtx != NULL) {
		path = ConfigManager::lookupErrorPage(code, locCtx->opErrorPageDirective);
		if (!path.empty()) {
			return path;
		}
	}

	// 2. Server 컨텍스트에서 탐색.
	path = ConfigManager::lookupErrorPage(code, serverCtx->opErrorPageDirective);
	if (!path.empty()) {
		return path;
	}

	// 3. 최상위 Http 컨텍스트에서 탐색.
	if (ConfigManager::_global_config != NULL) {
		path = ConfigManager::lookupErrorPage(code, ConfigManager::_global_config->httpContext.opErrorPageDirective);
		if (!path.empty()) {
			return path;
		}
	}

	// 모든 컨텍스트에서 경로를 찾지 못함.
	return "";
}
