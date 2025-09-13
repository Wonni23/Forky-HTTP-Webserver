#include "ConfApplicator.hpp"
#include <sstream>

ConfigDTO* ConfApplicator::_global_config = 0;

ConfApplicator::ConfApplicator() {}

ConfApplicator::~ConfApplicator() {
}

bool ConfApplicator::applyConfig(Server* server, const ConfigDTO& config) {
	// 1. Cascading이 완료된 최종 설정을 전역으로 저장.
	ConfApplicator::setGlobalConfig(config);

	if (ConfApplicator::getGlobalConfig() == 0) {
		ERROR_LOG("Failed to set global configuration");
		return false;
	}

	std::vector<ServerContext>& servers = ConfApplicator::getGlobalConfig()->httpContext.serverContexts;

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

void ConfApplicator::setGlobalConfig(const ConfigDTO& config) {
	if (_global_config != 0) {
		delete _global_config;
	}
	_global_config = new ConfigDTO(config);
}

ConfigDTO* ConfApplicator::getGlobalConfig() {
	return _global_config;
}

// const ServerContext* ConfApplicator::findServerForRequest(const HttpRequest* request, int connected_port) {
// 	if (_global_config == 0) return 0;

// 	std::string host_header = request->getHeader("Host");
// 	// Host 헤더에서 포트 번호 제거 (e.g., "example.com:8080" -> "example.com")
// 	std::string::size_type colon_pos = host_header.find(':');
// 	if (colon_pos != std::string::npos) {
// 		host_header = host_header.substr(0, colon_pos);
// 	}

// 	const std::vector<ServerContext>& servers = _global_config->httpContext.serverContexts;
// 	const ServerContext* default_server = 0;
	
// 	// 1. Host 헤더와 포트 번호가 정확히 일치하는 서버를 찾는다.
// 	for (size_t i = 0; i < servers.size(); ++i) {
// 		const ServerContext& server = servers[i];
// 		if (server.opListenDirective.empty()) continue;

// 		if (server.opListenDirective[i].port == connected_port) {
// 			if (!server.opServerNameDirective.empty() && server.opServerNameDirective[0].name == host_header) {
// 				return &server; // 정확한 매치
// 			}
// 			if (server.opListenDirective[0].default_server && default_server == 0) {
// 				default_server = &server; // 첫 번째 default_server 저장
// 			}
// 		}
// 	}

// 	// 2. 일치하는 서버가 없으면 default_server를 반환.
// 	if (default_server != 0) {
// 		return default_server;
// 	}

// 	// 3. default_server도 없으면 해당 포트의 첫 번째 서버를 반환.
// 	for (size_t i = 0; i < servers.size(); ++i) {
// 		const ServerContext& server = servers[i];
// 		if (server.opListenDirective.empty()) continue;
		
// 		if (server.opListenDirective[i].port == connected_port) {
// 			return &server;
// 		}
// 	}

// 	return 0; // 해당 포트에 연결된 서버가 없음 (이론적으로는 발생하면 안 됨)
// }

// const LocationContext* ConfApplicator::findLocationForRequest(const ServerContext* server, const std::string& uri) {
// 	if (server == 0) return 0;

// 	const LocationContext* best_match = 0;
// 	size_t best_match_length = 0;

// 	// 가장 긴 경로와 일치하는 location을 찾음.
// 	for (size_t i = 0; i < server->locationContexts.size(); ++i) {
// 		const LocationContext& location = server->locationContexts[i];
		
// 		if (uri.find(location.path) == 0) {
// 			if (location.path.length() > best_match_length) {
// 				best_match = &location;
// 				best_match_length = location.path.length();
// 			}
// 		}
// 	}
	
// 	return best_match; // 일치하는 location이 없으면 0을 반환
// }
