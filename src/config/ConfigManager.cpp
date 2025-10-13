#include "config/ConfigManager.hpp"
#include "config/ConfApplicator.hpp"  // [추가] 전역 설정 접근용
#include "http/HttpRequest.hpp"
#include "server/Server.hpp"
#include <sstream>


ConfigManager::ConfigManager() {}
ConfigManager::~ConfigManager() {}

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
	const ConfigDTO* globalConfig = ConfApplicator::getGlobalConfig();
	if (globalConfig != NULL) {
		path = ConfigManager::lookupErrorPage(code, globalConfig->httpContext.opErrorPageDirective);
		if (!path.empty()) {
			return path;
		}
	}

	// 모든 컨텍스트에서 경로를 찾지 못함.
	return "";
}
