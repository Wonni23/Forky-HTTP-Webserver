#ifndef CGI_EXECUTOR_HPP
#define CGI_EXECUTOR_HPP

#include <string>
#include <vector>
#include <map>
#include "http/HttpRequest.hpp"
#include "dto/ConfigDTO.hpp"

/**
 * @brief CGI 프로그램을 fork-exec 방식으로 실행하고,
 * 그 결과를 문자열로 반환하는 CGI 실행 전문 클래스.
 *
 * HttpController로부터 CGI 실행 임무를 위임받아,
 * 환경변수 설정, 파이프 생성, 프로세스 생성 및 실행,
 * 결과 수집 등 모든 복잡한 OS 레벨 작업을 처리함.
 */
class CgiExecutor {
private:
	// --- 멤버 변수 ---
	const HttpRequest* _request;
	std::string _cgiPath;
	const ServerContext* _serverConf;
	const LocationContext* _locConf;

	char** _envp; // execve에 전달할 환경변수 배열

	// --- 헬퍼 함수 ---

	/**
	 * @brief CGI 표준에 따라 환경변수를 설정.
	 *
	 * REQUEST_METHOD, QUERY_STRING, CONTENT_LENGTH 등
	 * CGI 스크립트가 필요로 하는 모든 환경변수를 생성함.
	 */
	void setupEnvironment();

	/**
	 * @brief _envp 배열을 위해 동적 할당된 메모리를 해제.
	 */
	void clearEnvironment();

public:
	/**
	 * @brief CgiExecutor 생성자.
	 *
	 * 실행에 필요한 모든 정보를 수령함.
	 * @param request HTTP 요청 정보.
	 * @param cgiPath 실행할 CGI 스크립트의 절대 경로.
	 * @param serverConf 서버 설정.
	 * @param locConf 로케이션 설정.
	 */
	CgiExecutor(const HttpRequest* request, const std::string& cgiPath,
				const ServerContext* serverConf, const LocationContext* locConf);

	/**
	 * @brief 소멸자. 동적으로 할당된 자원(환경변수 배열 등)을 해제.
	 */
	~CgiExecutor();

	/**
	 * @brief CGI 프로그램을 실행.
	 *
	 * @return CGI 스크립트가 stdout으로 출력한 모든 내용 (헤더 + 바디).
	 *         실패 시 빈 문자열을 반환함.
	 */
	std::string execute();
};

#endif
