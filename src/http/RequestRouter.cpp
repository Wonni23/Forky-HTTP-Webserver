#include "http/RequestRouter.hpp"
#include "config/ConfApplicator.hpp"
#include <algorithm>

const ServerContext* RequestRouter::findServerForRequest(const HttpRequest* request, int connected_port) {
	if (request == NULL || connected_port == 0) {
		ERROR_LOG("[RequestRouter] Parameter is NULL");
		return NULL;
	}
	
	DEBUG_LOG("[RequestRouter] ===== Finding server for request =====");
	DEBUG_LOG("[RequestRouter] Connected port: " << connected_port);
	
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
	DEBUG_LOG("[RequestRouter] Host header: " << (host_header.empty() ? "(empty)" : host_header));
	
	std::string::size_type colon_pos = host_header.find(':');
	if (colon_pos != std::string::npos) {
		std::string original_host = host_header;
		host_header = host_header.substr(0, colon_pos);
		DEEP_LOG("[RequestRouter] Host header stripped port: " << original_host << " -> " << host_header);
	}

	const std::vector<ServerContext>& servers = global_config->httpContext.serverContexts;
	if (servers.empty()) {
		ERROR_LOG("[RequestRouter] No servers configured");
		return NULL;
	}
	
	DEBUG_LOG("[RequestRouter] Total servers in config: " << servers.size());
	
	const ServerContext* default_server = NULL;
	const ServerContext* first_port_match = NULL;

	int server_index = 0;
	for (std::vector<ServerContext>::const_iterator it = servers.begin(); it != servers.end(); ++it) {
		const ServerContext& server = *it;
		
		DEEP_LOG("[RequestRouter] Checking server #" << server_index);

		if (server.opListenDirective.empty()) {
			DEEP_LOG("[RequestRouter] Server #" << server_index << " has no listen directives, skipping");
			server_index++;
			continue;
		}

		// listen 지시어 순회
		int listen_index = 0;
		for (std::vector<ListenDirective>::const_iterator listen_it = server.opListenDirective.begin(); 
			 listen_it != server.opListenDirective.end(); ++listen_it) {
			const ListenDirective& listen = *listen_it;
			
			DEEP_LOG("[RequestRouter] Server #" << server_index << " listen #" << listen_index 
					 << ": port=" << listen.port 
					 << " default_server=" << listen.default_server);
			
			if (listen.port != connected_port) {
				DEEP_LOG("[RequestRouter] Port mismatch: " << listen.port << " != " << connected_port);
				listen_index++;
				continue;
			}

			DEEP_LOG("[RequestRouter] Port matched: " << listen.port);

			// 1. Host 헤더와 포트가 모두 일치하는 서버 우선 탐색
			if (!server.opServerNameDirective.empty()) {
				std::string server_name = server.opServerNameDirective[0].name;
				DEEP_LOG("[RequestRouter] Server name: " << server_name << " vs Host: " << host_header);
				
				if (server_name == host_header) {
					DEBUG_LOG("[RequestRouter] Exact match found! server #" << server_index << " name=" << server_name);
					return &server; // 가장 정확한 매치
				}
			} else {
				DEEP_LOG("[RequestRouter] Server #" << server_index << " has no server_name directive");
			}

			// 2. default_server 저장 (첫 번째 것만)
			if (listen.default_server && default_server == NULL) {
				DEBUG_LOG("[RequestRouter] Found default_server: server #" << server_index);
				default_server = &server;
			}
			
			// 3. 포트만 일치하는 첫 번째 서버 저장
			if (first_port_match == NULL) {
				DEEP_LOG("[RequestRouter] First port match: server #" << server_index);
				first_port_match = &server;
			}
			
			listen_index++;
		}
		
		server_index++;
	}

	// 4. 우선순위에 따라 반환
	if (default_server != NULL) {
		DEBUG_LOG("[RequestRouter] Using default_server");
		return default_server;
	}
	
	if (first_port_match != NULL) {
		DEBUG_LOG("[RequestRouter] Using first port match");
		return first_port_match;
	}

	ERROR_LOG("[RequestRouter] No matching server found for port=" << connected_port << " host=" << host_header);
	return NULL;
}


const LocationContext* RequestRouter::findLocationForRequest(const ServerContext* server, const std::string& uri, const std::string& method) {
	DEBUG_LOG("[RequestRouter] ===== Finding location for URI =====");
	DEBUG_LOG("[RequestRouter] URI: " << uri << " Method: " << method);
	
	if (server == NULL) {
		ERROR_LOG("[RequestRouter] ServerContext is NULL");
		return NULL;
	}

	const LocationContext* longest_match = NULL;
	const LocationContext* best_match = NULL;
	size_t longest_length = 0;
	size_t best_length = 0;

	const std::vector<LocationContext>& locations = server->locationContexts;
	DEBUG_LOG("[RequestRouter] Total locations in server: " << locations.size());

	int loc_index = 0;
	for (std::vector<LocationContext>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
		const LocationContext& loc = *it;
		
		DEEP_LOG("[RequestRouter] Checking location #" << loc_index << ": path=" << loc.path);
		
		bool is_match = false;
		
		if (!loc.path.empty() && loc.path[loc.path.length() - 1] == '/') {
			if (uri.compare(0, loc.path.length(), loc.path) == 0) {
				is_match = true;
			} else if (uri + "/" == loc.path) {
				is_match = true;
			}
		} else {
			if (uri.compare(0, loc.path.length(), loc.path) == 0) {
				if (uri.length() == loc.path.length() || 
					(uri.length() > loc.path.length() && uri[loc.path.length()] == '/')) {
					is_match = true;
				}
			}
		}
		
		if (is_match) {
			DEEP_LOG("[RequestRouter] Location #" << loc_index << " matches! path=" << loc.path 
					 << " length=" << loc.path.length());
			
			if (loc.path.length() > longest_length) {
				longest_length = loc.path.length();
				longest_match = &loc;
				DEEP_LOG("[RequestRouter] New longest match: " << loc.path);
			}
			
			bool method_allowed = isMethodAllowedInLocation(method, loc);
			
			if (method_allowed && loc.path.length() > best_length) {
				best_length = loc.path.length();
				best_match = &loc;
				DEEP_LOG("[RequestRouter] New best match (method allowed): " << loc.path);
			}
		} else {
			DEEP_LOG("[RequestRouter] Location #" << loc_index << " does not match");
		}
		
		loc_index++;
	}
	
	if (best_match != NULL) {
		DEBUG_LOG("[RequestRouter] Best location match (method allowed): path=" << best_match->path << " length=" << best_length);
		return best_match;
	}
	
	if (longest_match != NULL) {
		DEBUG_LOG("[RequestRouter] Longest URI match (method NOT allowed): path=" << longest_match->path << " length=" << longest_length);
		return longest_match;
	}
	
	ERROR_LOG("[RequestRouter] No matching location found for URI: " << uri);
	return NULL;
}


bool RequestRouter::isMethodAllowedInLocation(const std::string& method, const LocationContext& loc) {
	if (loc.opLimitExceptDirective.empty()) {
		return true;
	}
	
	const std::set<std::string>& allowed = loc.opLimitExceptDirective[0].allowed_methods;
	
	if (allowed.find(method) != allowed.end()) {
		return true;
	}

	return false;
}
