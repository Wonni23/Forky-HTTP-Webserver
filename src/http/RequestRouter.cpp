#include "http/RequestRouter.hpp"
#include "config/ConfApplicator.hpp"

const ServerContext* RequestRouter::findServerForRequest(const HttpRequest* request, int connected_port) {
	const ConfigDTO* global_config = ConfApplicator::getGlobalConfig();
	if (global_config == NULL) {
		ERROR_LOG("[RequestRouter] Global config is NULL");
		return NULL;
	}

	if (request == NULL) {
		ERROR_LOG("[RequestRouter] Request is NULL");
		return NULL;
	}

	std::string host_header = request->getHeader("Host");
	std::string::size_type colon_pos = host_header.find(':');
	if (colon_pos != std::string::npos) {
		host_header = host_header.substr(0, colon_pos);
	}

	const std::vector<ServerContext>& servers = global_config->httpContext.serverContexts;
	const ServerContext* default_server = NULL;
	const ServerContext* first_port_match = NULL;

	for (std::vector<ServerContext>::const_iterator it = servers.begin(); it != servers.end(); ++it) {
		const ServerContext& server = *it;

		if (server.opListenDirective.empty()) continue;

		// listen 지시어 순회
		for (std::vector<ListenDirective>::const_iterator listen_it = server.opListenDirective.begin(); listen_it != server.opListenDirective.end(); ++listen_it) {
			const ListenDirective& listen = *listen_it;
			if (listen.port != connected_port) continue;

			// 1. Host 헤더와 포트가 모두 일치하는 서버 우선 탐색
			if (!server.opServerNameDirective.empty() && server.opServerNameDirective[0].name == host_header) {
				return &server; // 가장 정확한 매치
			}

			// 2. default_server 저장 (첫 번째 것만)
			if (listen.default_server && default_server == NULL) {
				default_server = &server;
			}
			
			// 3. 포트만 일치하는 첫 번째 서버 저장
			if (first_port_match == NULL) {
				first_port_match = &server;
			}
		}
	}

	// 4. 우선순위에 따라 반환
	if (default_server != NULL) return default_server;
	if (first_port_match != NULL) return first_port_match;

	ERROR_LOG("[RequestRouter] No matching server found - returning NULL");
	return NULL;
}

const LocationContext* RequestRouter::findLocationForRequest(const ServerContext* server, const std::string& uri) {
	if (server == NULL) {
		ERROR_LOG("[RequestRouter] ServerContext is NULL");
		return NULL;
	}

	const LocationContext* best_match = NULL;
	size_t best_length = 0;

	const std::vector<LocationContext>& locations = server->locationContexts;
	for (std::vector<LocationContext>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
		const LocationContext& loc = *it;
		
		if (uri.compare(0, loc.path.length(), loc.path) == 0) {
			if (loc.path.length() > best_length) {
				best_length = loc.path.length();
				best_match = &loc;
			}
		}
	}
	
	return best_match;
}

