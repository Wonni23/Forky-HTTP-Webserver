#include "cgi/CgiExecuter.hpp"
#include "http/HttpRequest.hpp"
#include "utils/Common.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <iostream>

// =========================================================================
// Static Helper Functions
// =========================================================================

/**
 * @brief 파일 경로에서 디렉토리 부분을 추출
 * @param path /var/www/cgi-bin/test.py
 * @return /var/www/cgi-bin
 */
static std::string getDirectoryFromPath(const std::string& path) {
	size_t lastSlash = path.find_last_of('/');
	if (lastSlash == std::string::npos) {
		return "."; // 슬래시가 없으면 현재 디렉토리
	}
	return path.substr(0, lastSlash);
}

/**
 * @brief 파일 경로에서 파일명만 추출
 * @param path /var/www/cgi-bin/test.py
 * @return test.py
 */
static std::string getFileNameFromPath(const std::string& path) {
	size_t lastSlash = path.find_last_of('/');
	if (lastSlash == std::string::npos) {
		return path;
	}
	return path.substr(lastSlash + 1);
}

/**
 * @brief 파일 확장자 기반 인터프리터 경로 반환
 * @param scriptPath CGI 스크립트 경로
 * @return 인터프리터 경로 (없으면 빈 문자열)
 */
static std::string getInterpreter(const std::string& scriptPath) {
	// 확장자 추출
	size_t dotPos = scriptPath.find_last_of('.');
	if (dotPos == std::string::npos) {
		return ""; // 확장자 없음 → 실행 파일로 간주
	}

	std::string ext = scriptPath.substr(dotPos);

	// 확장자에 따라 인터프리터 매핑
	if (ext == ".py") {
		return "/usr/bin/python3";
	} else if (ext == ".php") {
		return "/usr/bin/php-cgi";
	} else if (ext == ".sh") {
		return "/bin/sh";
	}

	return ""; // 알 수 없는 확장자 → 실행 파일로 간주
}

/**
 * @brief std::string을 C 문자열로 복사 (strdup 대체)
 */
static char* stringDup(const std::string& str) {
	char* result = new char[str.length() + 1];
	std::strcpy(result, str.c_str());
	return result;
}

/**
 * @brief HTTP 헤더명을 CGI 환경변수명으로 변환
 * @param headerName 원본 헤더명 (예: "User-Agent", "X-Test-Header")
 * @return CGI 환경변수명 (예: "HTTP_USER_AGENT", "HTTP_X_TEST_HEADER")
 */
static std::string headerToCgiEnvName(const std::string& headerName) {
	std::string result = "HTTP_";
	for (size_t i = 0; i < headerName.length(); ++i) {
		char c = headerName[i];
		if (c == '-') {
			result += '_';
		} else if (c >= 'a' && c <= 'z') {
			result += (c - 'a' + 'A'); // 소문자를 대문자로
		} else if (c >= 'A' && c <= 'Z') {
			result += c; // 이미 대문자
		} else {
			result += c; // 숫자나 기타 문자는 그대로
		}
	}
	return result;
}

// =========================================================================
// CgiExecutor Implementation
// =========================================================================

CgiExecutor::CgiExecutor(const HttpRequest* request, const std::string& cgiPath,
						 const ServerContext* serverConf, const LocationContext* locConf)
	: _request(request), _cgiPath(cgiPath), _serverConf(serverConf), _locConf(locConf), _envp(NULL) {
	// 환경변수 설정
	setupEnvironment();
}

CgiExecutor::~CgiExecutor() {
	clearEnvironment();
}

void CgiExecutor::setupEnvironment() {
	std::vector<std::string> envList;

	// 0. REDIRECT_STATUS (required for PHP-CGI security) // 이게 무조건 200이어야 하는가?
	envList.push_back("REDIRECT_STATUS=200");

	// 1. CGI/1.1 필수 환경변수
	envList.push_back("GATEWAY_INTERFACE=CGI/1.1");
	envList.push_back("SERVER_PROTOCOL=HTTP/1.1");

	// 2. REQUEST_METHOD
	envList.push_back("REQUEST_METHOD=" + _request->getMethod());

	// 3. QUERY_STRING
	std::string uri = _request->getUri();
	size_t qPos = uri.find('?');
	if (qPos != std::string::npos) {
		envList.push_back("QUERY_STRING=" + uri.substr(qPos + 1));
	} else {
		envList.push_back("QUERY_STRING=");
	}

	// 4. CONTENT_LENGTH (POST인 경우)
	// Chunked 인코딩의 경우 Content-Length 헤더가 없으므로, 실제 body 크기를 사용
	size_t contentLength = _request->getBody().length();
	DEBUG_LOG("[CgiExecutor] Setting CONTENT_LENGTH=" << contentLength);
	DEBUG_LOG("[CgiExecutor] Has Content-Length header: " << (_request->hasHeader("content-length") ? "YES" : "NO"));

	std::stringstream ss;
	ss << contentLength;
	envList.push_back("CONTENT_LENGTH=" + ss.str());

	// 5. CONTENT_TYPE
	std::string contentType = _request->getHeader("Content-Type");
	if (!contentType.empty()) {
		envList.push_back("CONTENT_TYPE=" + contentType);
	} else {
		envList.push_back("CONTENT_TYPE=");
	}

	// 6. SERVER_NAME
	if (!_serverConf->opServerNameDirective.empty()) {
		envList.push_back("SERVER_NAME=" + _serverConf->opServerNameDirective[0].name);
	} else {
		envList.push_back("SERVER_NAME=localhost");
	}

	// 7. SERVER_PORT
	if (!_serverConf->opListenDirective.empty()) {
		std::stringstream portSs;
		portSs << _serverConf->opListenDirective[0].port;
		envList.push_back("SERVER_PORT=" + portSs.str());
	} else {
		envList.push_back("SERVER_PORT=80"); // 이거는 기본이 80이 맞나?
	}

	// 8. SCRIPT_NAME, SCRIPT_FILENAME, DOCUMENT_ROOT
	// ubuntu_cgi_tester는 이 환경변수들이 있으면 PATH_INFO와의 관계를 검증함
	// std::string scriptName = uri;
	// size_t scriptQPos = scriptName.find('?');
	// if (scriptQPos != std::string::npos) {
	// 	scriptName = scriptName.substr(0, scriptQPos);
	// }
	// envList.push_back("SCRIPT_NAME=" + scriptName);
	// envList.push_back("SCRIPT_FILENAME=" + _cgiPath);
	//
	// if (!_locConf->opRootDirective.empty()) {
	// 	envList.push_back("DOCUMENT_ROOT=" + _locConf->opRootDirective[0].path);
	// } else if (!_serverConf->opRootDirective.empty()) {
	// 	envList.push_back("DOCUMENT_ROOT=" + _serverConf->opRootDirective[0].path);
	// }

	// 9. PATH_INFO (location type에 따라 다르게 처리)
	std::string pathInfo;

	if (_locConf->matchType == MATCH_EXTENSION) {
		// Extension location: PATH_INFO = root + filename
		// 예: URI=/directory/youpi.bla, root=/home/.../YoupiBanane -> PATH_INFO=/home/.../YoupiBanane/youpi.bla
		if (!_locConf->opRootDirective.empty()) {
			std::string fileName = uri.substr(uri.find_last_of('/'));
			pathInfo = _locConf->opRootDirective[0].path + fileName;

			// QUERY_STRING 제거
			size_t queryPos = pathInfo.find('?');
			if (queryPos != std::string::npos) {
				pathInfo = pathInfo.substr(0, queryPos);
			}
		}
	} else if (_locConf->matchType == MATCH_PREFIX) {
		// Prefix location: 표준 CGI PATH_INFO (URI에서 location path 제거)
		// 예: URI=/cgi-bin/script.py/extra, location=/cgi-bin/ -> PATH_INFO=script.py/extra
		if (uri.find(_locConf->path) == 0) {
			pathInfo = uri.substr(_locConf->path.length());

			// QUERY_STRING 제거
			size_t queryPos = pathInfo.find('?');
			if (queryPos != std::string::npos) {
				pathInfo = pathInfo.substr(0, queryPos);
			}
		}
	}

	envList.push_back("PATH_INFO=" + pathInfo);

	// 10. All HTTP headers to HTTP_* environment variables (RFC 3875)
	// Content-Length와 Content-Type은 이미 별도의 CGI 환경변수로 처리되었으므로 제외
	const std::map<std::string, std::string>& headers = _request->getHeaders();
	for (std::map<std::string, std::string>::const_iterator it = headers.begin();
	     it != headers.end(); ++it) {
		const std::string& headerName = it->first;
		const std::string& headerValue = it->second;

		// 헤더명을 소문자로 변환하여 비교 (대소문자 무관)
		std::string lowerName = headerName;
		for (size_t i = 0; i < lowerName.length(); ++i) {
			if (lowerName[i] >= 'A' && lowerName[i] <= 'Z') {
				lowerName[i] = lowerName[i] - 'A' + 'a';
			}
		}

		// Content-Length와 Content-Type은 CONTENT_LENGTH, CONTENT_TYPE으로 별도 처리되므로 건너뛰기
		if (lowerName == "content-length" || lowerName == "content-type") {
			continue;
		}

		// HTTP_ prefix를 붙여서 환경변수로 추가
		std::string envName = headerToCgiEnvName(headerName);
		envList.push_back(envName + "=" + headerValue);
	}

	DEBUG_LOG("=========== CgiExecutor.cpp setupEnvironment ===========");
	for (size_t i = 0; i < envList.size(); i++) {
		DEBUG_LOG(envList[i]);
	}
	DEBUG_LOG('\n');

	// vector를 char** 배열로 변환
	_envp = new char*[envList.size() + 1];
	for (size_t i = 0; i < envList.size(); ++i) {
		_envp[i] = stringDup(envList[i]);
	}
	_envp[envList.size()] = NULL; // NULL 종료
}

void CgiExecutor::clearEnvironment() {
	if (_envp) {
		for (size_t i = 0; _envp[i] != NULL; ++i) {
			delete[] _envp[i];
		}
		delete[] _envp;
		_envp = NULL;
	}
}

std::string CgiExecutor::execute() {
	// 1. 인터프리터 결정
	std::string interpreter = getInterpreter(_cgiPath);

	// 2. pipe 생성 (stdin, stdout, stderr용)
	int pipeStdin[2];
	int pipeStdout[2];
	int pipeStderr[2];

	if (pipe(pipeStdin) == -1 || pipe(pipeStdout) == -1 || pipe(pipeStderr) == -1) {
		// pipe 생성 실패
		return "";
	}

	// 3. fork
	pid_t pid = fork();

	if (pid == -1) {
		// fork 실패
		close(pipeStdin[0]);
		close(pipeStdin[1]);
		close(pipeStdout[0]);
		close(pipeStdout[1]);
		close(pipeStderr[0]);
		close(pipeStderr[1]);
		return "";
	}

	if (pid == 0) {
		// === 자식 프로세스 ===

		// stdin을 pipe로 연결
		dup2(pipeStdin[0], STDIN_FILENO);
		close(pipeStdin[0]);
		close(pipeStdin[1]);

		// stdout을 pipe로 연결
		dup2(pipeStdout[1], STDOUT_FILENO);
		close(pipeStdout[0]);
		close(pipeStdout[1]);

		// stderr을 pipe로 연결
		dup2(pipeStderr[1], STDERR_FILENO);
		close(pipeStderr[0]);
		close(pipeStderr[1]);

		// 스크립트가 있는 디렉토리로 이동 (상대경로 지원)
		std::string scriptDir = getDirectoryFromPath(_cgiPath);
		if (chdir(scriptDir.c_str()) != 0) {
			// chdir 실패
			exit(1);
		}

		// argv 배열 준비
		std::string scriptName = getFileNameFromPath(_cgiPath);
		char* argv[3];

		if (!interpreter.empty()) {
			// 인터프리터 실행: [인터프리터, 스크립트명, NULL]
			argv[0] = stringDup(interpreter);
			argv[1] = stringDup(scriptName);
			argv[2] = NULL;

			execve(interpreter.c_str(), argv, _envp);
		} else {
			// 실행 파일: [스크립트명, NULL]
			argv[0] = stringDup(scriptName);
			argv[1] = NULL;

			execve(_cgiPath.c_str(), argv, _envp);
		}

		// execve 실패 시 여기 도달
		exit(1);
	}

	// === 부모 프로세스 ===

	// 사용하지 않는 pipe 끝 닫기
	close(pipeStdin[0]);
	close(pipeStdout[1]);
	close(pipeStderr[1]);

	// stdin을 논블로킹으로 설정
	fcntl(pipeStdin[1], F_SETFL, O_NONBLOCK);
	fcntl(pipeStdout[0], F_SETFL, O_NONBLOCK);
	fcntl(pipeStderr[0], F_SETFL, O_NONBLOCK);

	// select()로 stdin write와 stdout/stderr read를 동시 처리
	const std::string& body = _request->getBody();
	size_t totalWritten = 0;
	std::string output;
	std::string errorOutput;
	char buffer[4096];

	// body가 비어있으면 즉시 stdin 닫기 (GET 요청 등)
	if (body.empty()) {
		close(pipeStdin[1]);
	}

	while (true) {
		fd_set readFds, writeFds;
		FD_ZERO(&readFds);
		FD_ZERO(&writeFds);

		int maxFd = -1;

		// stdout/stderr는 항상 읽기 대기
		FD_SET(pipeStdout[0], &readFds);
		FD_SET(pipeStderr[0], &readFds);
		maxFd = std::max(pipeStdout[0], pipeStderr[0]);

		// stdin에 쓸 데이터가 남아있으면 쓰기 대기
		bool stdinOpen = (!body.empty() && totalWritten < body.length());
		if (stdinOpen) {
			FD_SET(pipeStdin[1], &writeFds);
			maxFd = std::max(maxFd, pipeStdin[1]);
		}

		struct timeval timeout;
		timeout.tv_sec = CGI_TIMEOUT;
		timeout.tv_usec = 0;

		int ready = select(maxFd + 1, &readFds, &writeFds, NULL, &timeout);
		if (ready < 0) {
			ERROR_LOG("[CgiExecutor] select() failed");
			break;
		}
		if (ready == 0) {
			ERROR_LOG("[CgiExecutor] CGI timeout");
			break;
		}

		// stdin에 write 가능하면 write
		if (stdinOpen && FD_ISSET(pipeStdin[1], &writeFds)) {
			size_t toWrite = std::min((size_t)65536, body.length() - totalWritten);
			ssize_t written = write(pipeStdin[1], body.c_str() + totalWritten, toWrite);
			if (written > 0) {
				totalWritten += written;
			}
			if (totalWritten >= body.length()) {
				close(pipeStdin[1]); // 모두 썼으면 stdin 닫기
				stdinOpen = false;
			}
		}

		// stdout에서 read
		if (FD_ISSET(pipeStdout[0], &readFds)) {
			ssize_t bytesRead = read(pipeStdout[0], buffer, sizeof(buffer));
			if (bytesRead > 0) {
				output.append(buffer, bytesRead);
			} else if (bytesRead == 0) {
				// EOF - CGI 종료
				break;
			}
		}

		// stderr에서 read
		if (FD_ISSET(pipeStderr[0], &readFds)) {
			ssize_t bytesRead = read(pipeStderr[0], buffer, sizeof(buffer));
			if (bytesRead > 0) {
				errorOutput.append(buffer, bytesRead);
			}
		}
	}

	// 남은 stderr 읽기
	ssize_t bytesRead;
	while ((bytesRead = read(pipeStderr[0], buffer, sizeof(buffer))) > 0) {
		errorOutput.append(buffer, bytesRead);
	}

	close(pipeStdout[0]);
	close(pipeStderr[0]);

	// 자식 프로세스 종료 대기
	int status;
	waitpid(pid, &status, 0);

	// 자식이 정상 종료했는지 확인
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		// 비정상 종료
		ERROR_LOG("CGI script exited abnormally with status " << (WIFEXITED(status) ? WEXITSTATUS(status) : -1));
		if (!errorOutput.empty()) {
			ERROR_LOG("CGI stderr: " << errorOutput);
		}
		return "";
	}

	// stderr에 무언가 출력되었다면 로깅
	if (!errorOutput.empty()) {
		ERROR_LOG("CGI stderr: " << errorOutput);
	}

	return output;
}
