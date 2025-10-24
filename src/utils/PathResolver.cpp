#include "utils/PathResolver.hpp"
#include "utils/FileUtils.hpp"
#include "utils/Common.hpp"


// ========================================================================
// 메인 경로 해석 함수
// ========================================================================
std::string PathResolver::resolvePath(
	const ServerContext* server, 
	const LocationContext* loc, 
	const std::string& uri) 
{
	DEBUG_LOG("[PathResolver] Input URI: " << uri);
	
	if (server == NULL || loc == NULL) {
		ERROR_LOG("[PathResolver] NULL server or location context");
		return "";
	}

	DEBUG_LOG("[PathResolver] Location path: " << loc->path 
			  << " matchType: " << loc->matchType);

	// matchType에 따라 다른 처리
	switch (loc->matchType) {
		case MATCH_EXACT:
			return resolveExactPath(loc, uri);
			
		case MATCH_EXTENSION:
			return resolveExtensionPath(server, loc, uri);
			
		case MATCH_PREFIX:
			return resolvePrefixPath(server, loc, uri);
			
		default:
			ERROR_LOG("[PathResolver] Unknown match type: " << loc->matchType);
			return "";
	}
}

// ========================================================================
// MATCH_EXACT 처리
// ========================================================================
std::string PathResolver::resolveExactPath(
	const LocationContext* loc, 
	const std::string& uri) 
{
	DEBUG_LOG("[PathResolver] EXACT match - URI: " << uri);
	
	// EXACT 매칭: alias가 정확한 파일/디렉토리를 가리킴
	if (!loc->opAliasDirective.empty()) {
		std::string resolved = loc->opAliasDirective[0].path;
		DEBUG_LOG("[PathResolver] Using alias: " << resolved);
		return FileUtils::normalizePath(resolved);
	}
	
	// alias 없으면 root 사용
	if (!loc->opRootDirective.empty()) {
		std::string resolved = loc->opRootDirective[0].path + uri;
		DEBUG_LOG("[PathResolver] Using root: " << resolved);
		return FileUtils::normalizePath(resolved);
	}
	
	ERROR_LOG("[PathResolver] EXACT match but no alias or root");
	return "";
}

// ========================================================================
// MATCH_EXTENSION 처리
// ========================================================================
std::string PathResolver::resolveExtensionPath(
	const ServerContext* server,
	const LocationContext* loc, 
	const std::string& uri) 
{
	DEBUG_LOG("[PathResolver] EXTENSION match - URI: " << uri 
			  << " Extension: " << loc->path);
	
	// EXTENSION 매칭: root + uri 방식으로 처리
	std::string root_path;
	
	if (!loc->opRootDirective.empty()) {
		root_path = loc->opRootDirective[0].path;
		DEBUG_LOG("[PathResolver] Using root from location: " << root_path);
	} else if (server != NULL && !server->opRootDirective.empty()) {
		root_path = server->opRootDirective[0].path;
		DEBUG_LOG("[PathResolver] Using root from server: " << root_path);
	} else {
		root_path = "/var/www/html";
		DEBUG_LOG("[PathResolver] Using default root: " << root_path);
	}
	
	std::string resolved = root_path + "/" + uri;
	
	DEBUG_LOG("[PathResolver] Resolved to: " << resolved);
	return FileUtils::normalizePath(resolved);
}

// ========================================================================
// MATCH_PREFIX 처리
// ========================================================================
std::string PathResolver::resolvePrefixPath(
	const ServerContext* server,
	const LocationContext* loc, 
	const std::string& uri) 
{
	DEBUG_LOG("[PathResolver] PREFIX match - URI: " << uri 
			  << " Location: " << loc->path);
	
	// 1. alias 우선 처리
	if (!loc->opAliasDirective.empty()) {
		return resolveWithAlias(loc, uri);
	}
	
	// 2. alias 없으면 root 사용
	return resolveWithRoot(server, loc, uri);
}

// ========================================================================
// alias를 사용한 경로 해석
// ========================================================================
std::string PathResolver::resolveWithAlias(
	const LocationContext* loc,
	const std::string& uri)
{
	const std::string& alias_path = loc->opAliasDirective[0].path;
	const std::string& location_path = loc->path;
	
	DEBUG_LOG("[PathResolver] Using alias: " << alias_path);
	DEBUG_LOG("[PathResolver] Location path: " << location_path);
	
	// URI가 location path로 시작하는지 확인
	if (uri.compare(0, location_path.length(), location_path) != 0) {
		if (uri + "/" == location_path) {
			DEBUG_LOG("[PathResolver] URI matches location (trailing slash case)");
			return FileUtils::normalizePath(alias_path);
		}
		
		ERROR_LOG("[PathResolver] URI does not match location path");
		return "";
	}
	
	// location path 제거하고 나머지 추출
	std::string remainder = uri.substr(location_path.length());
	DEBUG_LOG("[PathResolver] Remainder after location: " << remainder);
	
	std::string resolved = alias_path + "/" + remainder;
	
	DEBUG_LOG("[PathResolver] Alias resolved to: " << resolved);
	return FileUtils::normalizePath(resolved);
}

// ========================================================================
// root를 사용한 경로 해석
// ========================================================================
std::string PathResolver::resolveWithRoot(
	const ServerContext* server,
	const LocationContext* loc,
	const std::string& uri)
{
	std::string root_path;
	
	// root 우선순위: location > server > default
	if (!loc->opRootDirective.empty()) {
		root_path = loc->opRootDirective[0].path;
		DEBUG_LOG("[PathResolver] Using root from location: " << root_path);
	} else if (server != NULL && !server->opRootDirective.empty()) {
		root_path = server->opRootDirective[0].path;
		DEBUG_LOG("[PathResolver] Using root from server: " << root_path);
	} else {
		root_path = "/var/www/html";
		DEBUG_LOG("[PathResolver] Using default root: " << root_path);
	}
	
	std::string resolved = root_path + "/" + uri;
	
	DEBUG_LOG("[PathResolver] Root resolved to: " << resolved);
	return FileUtils::normalizePath(resolved);
}

// ========================================================================
// index 파일 찾기
// ========================================================================
std::string PathResolver::findIndexFile(
	const std::string& dirPath, 
	const LocationContext* loc)
{
	DEBUG_LOG("[PathResolver] Finding index file in: " << dirPath);
	
	if (loc == NULL || loc->opIndexDirective.empty()) {
		DEBUG_LOG("[PathResolver] No index directive");
		return "";
	}

	// 설정된 index 파일들을 순서대로 검사
	for (size_t i = 0; i < loc->opIndexDirective.size(); ++i) {
		std::string index_file = loc->opIndexDirective[i].filename;
		
		std::string full_path = dirPath + "/" + index_file;
		full_path = FileUtils::normalizePath(full_path);

		DEBUG_LOG("[PathResolver] Checking index file: " << full_path);

		// 파일 존재 여부 확인
		if (FileUtils::pathExists(full_path) && !FileUtils::isDirectory(full_path)) {
			DEBUG_LOG("[PathResolver] Found index file: " << full_path);
			return full_path;
		}
	}

	DEBUG_LOG("[PathResolver] No valid index file found");
	return "";
}