#include "config/ConfCascader.hpp"

ConfCascader::ConfCascader() {}

ConfCascader::~ConfCascader() {}

ConfigDTO ConfCascader::applyCascade(const ConfigDTO& originalConfig) const {
	ConfigDTO cascadedConfig = originalConfig;  // 복사본 생성

	// 각 서버에 대해 HTTP -> Server 상속 적용
	for (size_t i = 0; i < cascadedConfig.httpContext.serverContexts.size(); i++) {
		ServerContext& server = cascadedConfig.httpContext.serverContexts[i];

		// HTTP -> Server 상속
		cascadeHttpToServer(cascadedConfig.httpContext, server);

		// 각 location에 대해 Server -> Location 상속 적용
		for (size_t j = 0; j < server.locationContexts.size(); j++) {
			LocationContext& location = server.locationContexts[j];

			// Server -> Location 상속
			cascadeServerToLocation(server, location);

			// HTTP -> Location 직접 상속 (Server에 없는 경우)
			cascadeHttpToLocation(cascadedConfig.httpContext, location);
		}
	}

	// 상속이 끝난 후 기본값 적용 (nginx 동작 방식)
	applyDefaultValues(cascadedConfig);

	return cascadedConfig;
}

void ConfCascader::cascadeHttpToServer(const HttpContext& http, ServerContext& server) const {
	cascadeDirective(http.opBodySizeDirective, server.opBodySizeDirective, "client_max_body_size");
	cascadeDirective(http.opRootDirective, server.opRootDirective, "root");
	cascadeDirective(http.opIndexDirective, server.opIndexDirective, "index");
	cascadeErrorPage(http.opErrorPageDirective, server.opErrorPageDirective);
}

void ConfCascader::cascadeServerToLocation(const ServerContext& server, LocationContext& location) const {
	cascadeDirective(server.opBodySizeDirective, location.opBodySizeDirective, "client_max_body_size");
	cascadeDirective(server.opRootDirective, location.opRootDirective, "root");
	cascadeDirective(server.opIndexDirective, location.opIndexDirective, "index");
	cascadeErrorPage(server.opErrorPageDirective, location.opErrorPageDirective);
	cascadeDirective(server.opAutoindexDirective, location.opAutoindexDirective, "autoindex");
}

void ConfCascader::cascadeHttpToLocation(const HttpContext& http, LocationContext& location) const {
	// Server에도 없고 Location에도 없는 경우 HTTP에서 직접 상속
	cascadeDirective(http.opBodySizeDirective, location.opBodySizeDirective, "client_max_body_size");
	cascadeDirective(http.opRootDirective, location.opRootDirective, "root");
	cascadeDirective(http.opIndexDirective, location.opIndexDirective, "index");
	cascadeErrorPage(http.opErrorPageDirective, location.opErrorPageDirective);
}

ServerContext ConfCascader::cascadeToServer(const HttpContext& http, const ServerContext& server) const {
	ServerContext cascadedServer = server;  // 복사본 생성
	cascadeHttpToServer(http, cascadedServer);
	
	return cascadedServer;
}

LocationContext ConfCascader::cascadeToLocation(const HttpContext& http, 
											   const ServerContext& server, 
											   const LocationContext& location) const {
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
	}
}

void ConfCascader::cascadeRoot(const std::vector<RootDirective>& parent,
							  std::vector<RootDirective>& child) const {
	if (child.empty() && !parent.empty()) {
		child = parent;
	}
}

void ConfCascader::cascadeIndex(const std::vector<IndexDirective>& parent,
							   std::vector<IndexDirective>& child) const {
	if (child.empty() && !parent.empty()) {
		child = parent;
	}
}

void ConfCascader::cascadeErrorPage(const std::vector<ErrorPageDirective>& parent,
								   std::vector<ErrorPageDirective>& child) const {
	if (child.empty() && !parent.empty()) {
		child = parent;
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
	}
}

void ConfCascader::cascadeAutoindex(const std::vector<AutoindexDirective>& parent,
								   std::vector<AutoindexDirective>& child) const {
	if (child.empty() && !parent.empty()) {
		child = parent;
	}
}

void ConfCascader::printCascadeInfo(const ConfigDTO& beforeConfig, const ConfigDTO& afterConfig) const {
	
	
	for (size_t i = 0; i < beforeConfig.httpContext.serverContexts.size(); i++) {
		const ServerContext& beforeServer = beforeConfig.httpContext.serverContexts[i];
		const ServerContext& afterServer = afterConfig.httpContext.serverContexts[i];
		
		
		// Body Size 비교
		if (beforeServer.opBodySizeDirective.empty() && !afterServer.opBodySizeDirective.empty()) {
		}
		
		// Location별 비교
		for (size_t j = 0; j < beforeServer.locationContexts.size(); j++) {
			const LocationContext& beforeLoc = beforeServer.locationContexts[j];
			const LocationContext& afterLoc = afterServer.locationContexts[j];
			
			
			if (beforeLoc.opBodySizeDirective.empty() && !afterLoc.opBodySizeDirective.empty()) {
			}
			
			if (beforeLoc.opRootDirective.empty() && !afterLoc.opRootDirective.empty()) {
			}
			
			if (beforeLoc.opIndexDirective.empty() && !afterLoc.opIndexDirective.empty()) {
			}
			
			if (beforeLoc.opAutoindexDirective.empty() && !afterLoc.opAutoindexDirective.empty()) {
			}
		}
	}
}

void ConfCascader::printContextComparison(const std::string& contextName,
										 const std::string& beforeInfo,
										 const std::string& afterInfo) const {
	(void)contextName;
	(void)beforeInfo;
	(void)afterInfo;
}

void ConfCascader::applyDefaultValues(ConfigDTO& config) const {

	// HTTP context에 index가 없으면 기본값 적용
	if (config.httpContext.opIndexDirective.empty()) {
		IndexDirective defaultIndex("index.html");
		config.httpContext.opIndexDirective.push_back(defaultIndex);
	}

	// 각 서버와 location에 대해 기본값 적용
	for (size_t i = 0; i < config.httpContext.serverContexts.size(); i++) {
		ServerContext& server = config.httpContext.serverContexts[i];

		// Server에 index가 없으면 기본값 적용 (이미 상속받았을 수도 있음)
		if (server.opIndexDirective.empty()) {
			IndexDirective defaultIndex("index.html");
			server.opIndexDirective.push_back(defaultIndex);
		}

		// 각 location에 대해 기본값 적용
		for (size_t j = 0; j < server.locationContexts.size(); j++) {
			LocationContext& location = server.locationContexts[j];

			// Location에 index가 없으면 기본값 적용 (이미 상속받았을 수도 있음)
			if (location.opIndexDirective.empty()) {
				IndexDirective defaultIndex("index.html");
				location.opIndexDirective.push_back(defaultIndex);
			}
		}
	}

}