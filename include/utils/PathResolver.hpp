#ifndef PATHRESOLVER_HPP
#define PATHRESOLVER_HPP

#include <string>
#include "dto/ConfigDTO.hpp"

class PathResolver {
public:
	// 메인 경로 해석 함수
	static std::string resolvePath(
		const ServerContext* server, 
		const LocationContext* loc, 
		const std::string& uri
	);
	
	// index 파일 찾기
	static std::string findIndexFile(
		const std::string& dirPath, 
		const LocationContext* loc
	);

private:
	// matchType별 경로 해석 함수
	static std::string resolveExactPath(
		const LocationContext* loc, 
		const std::string& uri
	);
	
	static std::string resolveExtensionPath(
		const ServerContext* server,
		const LocationContext* loc, 
		const std::string& uri
	);
	
	static std::string resolvePrefixPath(
		const ServerContext* server,
		const LocationContext* loc, 
		const std::string& uri
	);
	
	// alias/root 처리 헬퍼 함수
	static std::string resolveWithAlias(
		const LocationContext* loc,
		const std::string& uri
	);
	
	static std::string resolveWithRoot(
		const ServerContext* server,
		const LocationContext* loc,
		const std::string& uri
	);
};

#endif
