#include "config/ConfCascader.hpp"

ConfCascader::ConfCascader() {}

ConfCascader::~ConfCascader() {}

ConfigDTO ConfCascader::applyCascade(const ConfigDTO& originalConfig) const {
	std::cout << "[CASCADER] applyCascade 시작" << std::endl;
	
	ConfigDTO cascadedConfig = originalConfig;  // 복사본 생성
	
	// 각 서버에 대해 HTTP -> Server 상속 적용
	for (size_t i = 0; i < cascadedConfig.httpContext.serverContexts.size(); i++) {
		ServerContext& server = cascadedConfig.httpContext.serverContexts[i];
		std::cout << "[CASCADER] 서버 " << (i + 1) << " 상속 처리 시작" << std::endl;
		
		// HTTP -> Server 상속
		cascadeHttpToServer(cascadedConfig.httpContext, server);
		
		// 각 location에 대해 Server -> Location 상속 적용
		for (size_t j = 0; j < server.locationContexts.size(); j++) {
			LocationContext& location = server.locationContexts[j];
			std::cout << "[CASCADER] Location '" << location.path << "' 상속 처리 시작" << std::endl;
			
			// Server -> Location 상속
			cascadeServerToLocation(server, location);
			
			// HTTP -> Location 직접 상속 (Server에 없는 경우)
			cascadeHttpToLocation(cascadedConfig.httpContext, location);
			
			std::cout << "[CASCADER] Location '" << location.path << "' 상속 처리 완료" << std::endl;
		}
		
		std::cout << "[CASCADER] 서버 " << (i + 1) << " 상속 처리 완료" << std::endl;
	}
	
	std::cout << "[CASCADER] applyCascade 완료" << std::endl;
	return cascadedConfig;
}

void ConfCascader::cascadeHttpToServer(const HttpContext& http, ServerContext& server) const {
	std::cout << "[CASCADER] HTTP -> Server 상속 처리" << std::endl;

	cascadeDirective(http.opBodySizeDirective, server.opBodySizeDirective, "client_max_body_size");
	cascadeDirective(http.opRootDirective, server.opRootDirective, "root");
	cascadeDirective(http.opIndexDirective, server.opIndexDirective, "index");
	cascadeErrorPage(http.opErrorPageDirective, server.opErrorPageDirective);
}

void ConfCascader::cascadeServerToLocation(const ServerContext& server, LocationContext& location) const {
	std::cout << "[CASCADER] Server -> Location '" << location.path << "' 상속 처리" << std::endl;

	cascadeDirective(server.opBodySizeDirective, location.opBodySizeDirective, "client_max_body_size");
	cascadeDirective(server.opRootDirective, location.opRootDirective, "root");
	cascadeDirective(server.opIndexDirective, location.opIndexDirective, "index");
	cascadeErrorPage(server.opErrorPageDirective, location.opErrorPageDirective);
	cascadeDirective(server.opAutoindexDirective, location.opAutoindexDirective, "autoindex");
}

void ConfCascader::cascadeHttpToLocation(const HttpContext& http, LocationContext& location) const {
	std::cout << "[CASCADER] HTTP -> Location '" << location.path << "' 직접 상속 처리" << std::endl;

	// Server에도 없고 Location에도 없는 경우 HTTP에서 직접 상속
	cascadeDirective(http.opBodySizeDirective, location.opBodySizeDirective, "client_max_body_size");
	cascadeDirective(http.opRootDirective, location.opRootDirective, "root");
	cascadeDirective(http.opIndexDirective, location.opIndexDirective, "index");
	cascadeErrorPage(http.opErrorPageDirective, location.opErrorPageDirective);
}

ServerContext ConfCascader::cascadeToServer(const HttpContext& http, const ServerContext& server) const {
	std::cout << "[CASCADER] cascadeToServer 호출" << std::endl;
	
	ServerContext cascadedServer = server;  // 복사본 생성
	cascadeHttpToServer(http, cascadedServer);
	
	return cascadedServer;
}

LocationContext ConfCascader::cascadeToLocation(const HttpContext& http, 
											   const ServerContext& server, 
											   const LocationContext& location) const {
	std::cout << "[CASCADER] cascadeToLocation 호출 - '" << location.path << "'" << std::endl;
	
	LocationContext cascadedLocation = location;  // 복사본 생성
	
	// Server -> Location 상속
	cascadeServerToLocation(server, cascadedLocation);
	
	// HTTP -> Location 직접 상속 (Server에 없는 경우)
	cascadeHttpToLocation(http, cascadedLocation);
	
	return cascadedLocation;
}

void ConfCascader::cascadeBodySize(const std::vector<BodySizeDirective>& parent,
								  std::vector<BodySizeDirective>& child) const {
	if (child.empty() && !parent.empty()) {
		child = parent;
		std::cout << "[CASCADER] Body size inherited: " << parent[0].size << std::endl;
	}
}

void ConfCascader::cascadeRoot(const std::vector<RootDirective>& parent,
							  std::vector<RootDirective>& child) const {
	if (child.empty() && !parent.empty()) {
		child = parent;
		std::cout << "[CASCADER] Root path inherited: " << parent[0].path << std::endl;
	}
}

void ConfCascader::cascadeIndex(const std::vector<IndexDirective>& parent,
							   std::vector<IndexDirective>& child) const {
	if (child.empty() && !parent.empty()) {
		child = parent;
		std::cout << "[CASCADER] Index file inherited: " << parent[0].filename << std::endl;
	}
}

void ConfCascader::cascadeErrorPage(const std::vector<ErrorPageDirective>& parent,
								   std::vector<ErrorPageDirective>& child) const {
	if (child.empty() && !parent.empty()) {
		child = parent;
		std::cout << "[CASCADER] Error page inherited from parent ("
				  << parent.size() << " directives)" << std::endl;
		return;
	}

	// child가 있고 parent도 있으면 병합 (child 우선)
	if (!child.empty() && !parent.empty()) {
		// parent의 모든 매핑을 수집
		std::map<int, std::string> parentMap;
		for (size_t i = 0; i < parent.size(); ++i) {
			for (std::map<int, std::string>::const_iterator it = parent[i].errorPageMap.begin();
				 it != parent[i].errorPageMap.end(); ++it) {
				parentMap[it->first] = it->second;
			}
		}

		// child의 모든 매핑을 수집 (child가 우선이므로 덮어씀)
		std::map<int, std::string> childMap;
		for (size_t i = 0; i < child.size(); ++i) {
			for (std::map<int, std::string>::const_iterator it = child[i].errorPageMap.begin();
				 it != child[i].errorPageMap.end(); ++it) {
				childMap[it->first] = it->second;
			}
		}

		// parent 매핑 중 child에 없는 것만 추가
		for (std::map<int, std::string>::const_iterator it = parentMap.begin();
			 it != parentMap.end(); ++it) {
			if (childMap.find(it->first) == childMap.end()) {
				childMap[it->first] = it->second;
			}
		}

		// 병합된 맵을 다시 child vector로 변환 (하나의 directive로 통합)
		child.clear();
		ErrorPageDirective merged;
		merged.errorPageMap = childMap;
		child.push_back(merged);

		std::cout << "[CASCADER] Error page merged with parent (total "
				  << childMap.size() << " mappings)" << std::endl;
	}
}

void ConfCascader::cascadeAutoindex(const std::vector<AutoindexDirective>& parent,
								   std::vector<AutoindexDirective>& child) const {
	if (child.empty() && !parent.empty()) {
		child = parent;
		std::cout << "[CASCADER] Autoindex inherited: " << (parent[0].enabled ? "on" : "off") << std::endl;
	}
}

void ConfCascader::printCascadeInfo(const ConfigDTO& beforeConfig, const ConfigDTO& afterConfig) const {
	std::cout << "\n=== CASCADE COMPARISON ===" << std::endl;
	
	std::cout << "서버 개수: " << beforeConfig.httpContext.serverContexts.size() << std::endl;
	
	for (size_t i = 0; i < beforeConfig.httpContext.serverContexts.size(); i++) {
		const ServerContext& beforeServer = beforeConfig.httpContext.serverContexts[i];
		const ServerContext& afterServer = afterConfig.httpContext.serverContexts[i];
		
		std::cout << "\n--- 서버 " << (i + 1) << " 비교 ---" << std::endl;
		
		// Body Size 비교
		if (beforeServer.opBodySizeDirective.empty() && !afterServer.opBodySizeDirective.empty()) {
			std::cout << "  Body Size 상속: " << afterServer.opBodySizeDirective[0].size << std::endl;
		}
		
		// Location별 비교
		for (size_t j = 0; j < beforeServer.locationContexts.size(); j++) {
			const LocationContext& beforeLoc = beforeServer.locationContexts[j];
			const LocationContext& afterLoc = afterServer.locationContexts[j];
			
			std::cout << "  Location '" << beforeLoc.path << "':" << std::endl;
			
			if (beforeLoc.opBodySizeDirective.empty() && !afterLoc.opBodySizeDirective.empty()) {
				std::cout << "    Body Size 상속: " << afterLoc.opBodySizeDirective[0].size << std::endl;
			}
			
			if (beforeLoc.opRootDirective.empty() && !afterLoc.opRootDirective.empty()) {
				std::cout << "    Root 상속: " << afterLoc.opRootDirective[0].path << std::endl;
			}
			
			if (beforeLoc.opIndexDirective.empty() && !afterLoc.opIndexDirective.empty()) {
				std::cout << "    Index 상속: " << afterLoc.opIndexDirective[0].filename << std::endl;
			}
			
			if (beforeLoc.opAutoindexDirective.empty() && !afterLoc.opAutoindexDirective.empty()) {
				std::cout << "    Autoindex 상속: " << (afterLoc.opAutoindexDirective[0].enabled ? "on" : "off") << std::endl;
			}
		}
	}
}

void ConfCascader::printContextComparison(const std::string& contextName,
										 const std::string& beforeInfo,
										 const std::string& afterInfo) const {
	std::cout << "[CASCADER] " << contextName << " 변경:" << std::endl;
	std::cout << "  이전: " << beforeInfo << std::endl;
	std::cout << "  이후: " << afterInfo << std::endl;
}