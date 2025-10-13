#ifndef CONFIGMANAGER_HPP
#define CONFIGMANAGER_HPP

#include "webserv.hpp"
#include "dto/ConfigDTO.hpp"

class Server; // 전방 선언으로 헤더 의존성 감소

class ConfigManager {
private:
	ConfigManager();
	~ConfigManager();

	/**
	 * @brief ErrorPageDirective 벡터 내에서 에러 코드 경로 탐색. (비공개 헬퍼)
	 * @param code 찾을 HTTP 에러 코드.
	 * @param directives 탐색 대상 ErrorPageDirective 벡터.
	 * @return 발견된 경로. 없으면 빈 문자열 반환함.
	 */
	static std::string	lookupErrorPage(int code, const std::vector<ErrorPageDirective>& directives);

public:
	/**
	 * @brief 주어진 조건에 맞는 에러 페이지 경로 탐색.
	 *
	 * nginx 상속 규칙(location -> server -> http)에 따라 경로 조회.
	 * @param code 찾을 HTTP 에러 코드.
	 * @param serverCtx 현재 요청이 속한 ServerContext.
	 * @param locCtx 현재 요청이 속한 LocationContext (없을 경우 NULL).
	 * @return 발견된 에러 페이지 경로. 없으면 빈 문자열 반환함.
	 */
	static std::string	findErrorPagePath(int code, const ServerContext* serverCtx, const LocationContext* locCtx);
};

#endif

