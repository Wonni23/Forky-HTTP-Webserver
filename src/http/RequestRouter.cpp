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


const LocationContext* RequestRouter::findLocationForRequest(
	const ServerContext* server, 
	const std::string& uri, 
	const std::string& method) 
{
	DEBUG_LOG("[RequestRouter] ===== Finding location for URI =====");
	DEBUG_LOG("[RequestRouter] URI: " << uri << " Method: " << method);
	
	if (server == NULL) {
		ERROR_LOG("[RequestRouter] ServerContext is NULL");
		return NULL;
	}

	// 쿼리 스트링 제거
	std::string cleanUri = uri;
	size_t queryPos = uri.find('?');
	if (queryPos != std::string::npos) {
		cleanUri = uri.substr(0, queryPos);
	}

	const LocationContext* exactMatch = NULL;
	const LocationContext* longestPrefixMatch = NULL;
	size_t longestPrefixLength = 0;
	const LocationContext* extensionMatch = NULL;

	const std::vector<LocationContext>& locations = server->locationContexts;
	DEBUG_LOG("[RequestRouter] Total locations in server: " << locations.size());

	// 모든 location을 순회하며 매칭
	int loc_index = 0;
	for (std::vector<LocationContext>::const_iterator it = locations.begin(); 
		 it != locations.end(); ++it) {
		
		const LocationContext& loc = *it;
		
		DEEP_LOG("[RequestRouter] Checking location #" << loc_index 
				 << ": path=" << loc.path 
				 << " matchType=" << loc.matchType);

		// matchType에 따라 다른 매칭 로직
		switch (loc.matchType) {
			case MATCH_EXACT:
				// 정확히 일치
				if (cleanUri == loc.path) {
					DEEP_LOG("[RequestRouter] EXACT match: " << loc.path);
					exactMatch = &loc;
					// EXACT 찾으면 더 이상 검색 불필요 (최우선)
					// 하지만 루프는 계속 (break로 switch만 빠져나감)
				}
				break;

			case MATCH_EXTENSION:
				// 확장자 매칭
				if (cleanUri.length() >= loc.path.length()) {
					std::string uriExtension = cleanUri.substr(
						cleanUri.length() - loc.path.length()
					);
					if (uriExtension == loc.path) {
						DEEP_LOG("[RequestRouter] EXTENSION match: " << loc.path);
						extensionMatch = &loc;
					}
				}
				break;

			case MATCH_PREFIX:
				// PREFIX 매칭
				{
					bool prefix_match = false;
					
					// location path가 /로 끝나는 경우
					if (!loc.path.empty() && loc.path[loc.path.length() - 1] == '/') {
						if (cleanUri.compare(0, loc.path.length(), loc.path) == 0) {
							prefix_match = true;
						} else if (cleanUri + "/" == loc.path) {
							prefix_match = true;
						}
					} 
					// location path가 /로 끝나지 않는 경우
					else {
						if (cleanUri.compare(0, loc.path.length(), loc.path) == 0) {
							// 정확히 일치하거나, 다음 문자가 /인 경우만
							if (cleanUri.length() == loc.path.length() || 
								(cleanUri.length() > loc.path.length() && 
								 cleanUri[loc.path.length()] == '/')) {
								prefix_match = true;
							}
						}
					}
					
					if (prefix_match) {
						DEEP_LOG("[RequestRouter] PREFIX match: " << loc.path 
								 << " length=" << loc.path.length());
						
						// 가장 긴 PREFIX 저장
						if (loc.path.length() > longestPrefixLength) {
							longestPrefixLength = loc.path.length();
							longestPrefixMatch = &loc;
							DEEP_LOG("[RequestRouter] New longest prefix: " << loc.path);
						}
					}
				}
				break;

			default:
				ERROR_LOG("[RequestRouter] Unknown matchType: " << loc.matchType);
				break;
		}
		
		// EXACT 찾았으면 조기 종료 (더 이상 검색 의미 없음)
		if (exactMatch != NULL) {
			DEBUG_LOG("[RequestRouter] EXACT match found, stopping search");
			break;
		}
		
		loc_index++;
	}

	// 우선순위: EXACT > PREFIX (가장 긴 것) > EXTENSION
	const LocationContext* selected = NULL;
	
	if (exactMatch) {
		selected = exactMatch;
		DEBUG_LOG("[RequestRouter] Selected EXACT match: " << selected->path);
	} 
	else if (extensionMatch) {  // ← EXTENSION이 PREFIX보다 우선!
		selected = extensionMatch;
		DEBUG_LOG("[RequestRouter] Selected EXTENSION match: " << selected->path);
	} 
	else if (longestPrefixMatch) {
		selected = longestPrefixMatch;
		DEBUG_LOG("[RequestRouter] Selected PREFIX match: " << selected->path 
				  << " length=" << longestPrefixLength);
	}

	if (!selected) {
		ERROR_LOG("[RequestRouter] No matching location found for URI: " << cleanUri);
		return NULL;
	}

	// 메서드 허용 여부 로깅
	bool method_allowed = isMethodAllowedInLocation(method, *selected);
	if (!method_allowed) {
		DEBUG_LOG("[RequestRouter] Method " << method 
				  << " NOT allowed in location " << selected->path);
	} else {
		DEBUG_LOG("[RequestRouter] Method " << method 
				  << " allowed in location " << selected->path);
	}

	return selected;
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
