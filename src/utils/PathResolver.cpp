#include "utils/PathResolver.hpp"
#include "dto/ConfigDTO.hpp"
#include "utils/FileUtils.hpp"
#include "utils/Common.hpp"

std::string PathResolver::resolvePath(const ServerContext* server, const LocationContext* loc, const std::string& uri) {
	DEBUG_LOG("[PathResolver] Input URI: " << uri);
	if (server == NULL || loc == NULL) {
		return ""; // Cannot resolve path without server or location configuration.
	}

	// 1. Check for an 'alias' directive (highest priority).
	if (!loc->opAliasDirective.empty()) {
		const std::string& alias_path = loc->opAliasDirective[0].path;
		DEBUG_LOG("[PathResolver] Using alias: " << alias_path);
		const std::string& location_path = loc->path;
		
		// Check if the request URI starts with the location path.
		if (uri.compare(0, location_path.length(), location_path) == 0) {
			// Cut off the location part and append the remainder to the alias path.
			std::string remainder = uri.substr(location_path.length());
			
			std::string resolved = alias_path;
			if (resolved.length() > 0 && resolved[resolved.length() - 1] != '/' && !remainder.empty() && remainder[0] != '/') {
				resolved += '/';
			}
			resolved += remainder;
			
			// Clean up the path and return.
			DEBUG_LOG("[PathResolver] Alias resolved to: " << resolved);
			return FileUtils::normalizePath(resolved); // normalizePath cleans up items like double slashes.
		}
		// URI가 location path와 매칭되지 않거나, URI가 /directory이고 location이 /directory/인 경우
		else if (uri + "/" == location_path) {
			// URI=/directory, location=/directory/ 케이스
			DEBUG_LOG("[PathResolver] URI matches location without trailing slash");
			std::string resolved = alias_path;
			DEBUG_LOG("[PathResolver] Alias resolved to: " << resolved);
			return FileUtils::normalizePath(resolved);
		}
	}
	
	// 2. If no alias is found, process with the 'root' directive.
	std::string root_path;
	if (!loc->opRootDirective.empty()) {
		root_path = loc->opRootDirective[0].path;
		DEBUG_LOG("[PathResolver] Using root from location: " << root_path);
	} else if (!server->opRootDirective.empty()) {
		root_path = server->opRootDirective[0].path;
		DEBUG_LOG("[PathResolver] Using root from server: " << root_path);
	} else {
		root_path = "/var/www/html"; // Default value.
		DEBUG_LOG("[PathResolver] Using default root: " << root_path);
	}

	std::string resolved = root_path;
	if (resolved.length() > 0 && resolved[resolved.length() - 1] == '/' && uri.length() > 0 && uri[0] == '/') {
		// If root ends with '/' and the URI starts with '/', remove the duplicate slash.
		resolved += uri.substr(1);
	} else if (resolved.length() > 0 && resolved[resolved.length() - 1] != '/' && uri.length() > 0 && uri[0] != '/') {
		resolved += '/';
		resolved += uri;
	} else {
		resolved += uri;
	}

	return FileUtils::normalizePath(resolved);
}

std::string PathResolver::findIndexFile(const std::string& dirPath, const LocationContext* loc)
{
	if (loc == NULL || loc->opIndexDirective.empty()) {
		return ""; // index 지시어가 없으면 찾을 수 없음
	}

	for (size_t i = 0; i < loc->opIndexDirective.size(); ++i) {
		std::string index_file = loc->opIndexDirective[i].filename;
		std::string full_path = dirPath;
		if (full_path[full_path.length() - 1] != '/') {
			full_path += '/';
		}
		full_path += index_file;

		// FileUtils의 함수를 사용하여 파일 존재 여부 확인
		if (FileUtils::pathExists(full_path)) {
			return full_path; // 처음으로 찾은 유효한 index 파일 반환
		}
	}

	return ""; // 유효한 index 파일을 찾지 못함
}
