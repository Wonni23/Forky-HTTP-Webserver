#include "config/ConfigManager.hpp"
#include "http/HttpRequest.hpp"
#include <sstream>

ConfigDTO* ConfigManager::_global_config = 0;

ConfigManager::ConfigManager() {}

ConfigManager::~ConfigManager() {
}

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
