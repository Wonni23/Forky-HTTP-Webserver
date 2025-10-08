#ifndef PATH_RESOLVER_HPP
#define PATH_RESOLVER_HPP

#include <string>
#include <vector>

/*
 * C++98 표준을 준수하기 위해, 필요한 헤더와 전방 선언을 사용합니다.
 * 실제 프로젝트에서는 "ConfigDTO.hpp" 등을 포함해야 합니다.
 */
struct ServerContext;
struct LocationContext;

class PathResolver {
public:
	/*
	 * 요청 URI를 실제 파일 시스템 경로로 변환합니다.
	 * root와 alias 지시어를 처리합니다.
	 * 예: (uri: "/img/logo.png", location.path: "/img/", root: "/var/www") -> "/var/www/img/logo.png"
	 * 예: (uri: "/app/start", location.path: "/app/", alias: "/usr/local/app/run") -> "/usr/local/app/run/start"
	 */
	static std::string resolvePath(const ServerContext* server, const LocationContext* location, const std::string& uri);

	/*
	 * 디렉토리 경로와 location 설정을 받아, 유효한 index 파일 경로를 찾습니다.
	 * location의 index 지시어에 명시된 파일들을 순서대로 확인합니다.
	 * 예: (dirPath: "/var/www/html/", index: "index.html", "index.htm") -> "/var/www/html/index.html" (존재 시)
	 */
	static std::string findIndexFile(const std::string& dirPath, const LocationContext* location);

	/*
	 * 요청 URI가 CGI 실행 대상인지 확인하고, CGI 인터프리터 경로를 반환합니다.
	 * location의 cgi_pass와 cgi_param 같은 설정을 사용합니다.
	 * 예: (uri: "/cgi-bin/test.py", cgi_pass: ".py /usr/bin/python3") -> "/usr/bin/python3"
	 * @return CGI 인터프리터 경로, 대상이 아니면 빈 문자열
	 */
	static std::string getCgiInterpreter(const std::string& uri, const LocationContext* location);
};

#endif /* PATH_RESOLVER_HPP */
