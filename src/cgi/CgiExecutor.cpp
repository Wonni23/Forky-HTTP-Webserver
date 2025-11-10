#include "cgi/CgiExecutor.hpp"
#include "http/HttpRequest.hpp"
#include "utils/Common.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
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
 * @brief 파일 확장자 기반 인터프리터 경로 반환
 * @param scriptPath CGI 스크립트 경로
 * @return 인터프리터 경로 (없으면 빈 문자열)
 */
static std::string getInterpreter(const std::string& scriptPath, const LocationContext* locConf) {
	// 확장자 추출
	size_t dotPos = scriptPath.find_last_of('.');
	if (dotPos == std::string::npos) {
		// 확장자 없음: cgi_pass 사용 가능하면 사용, 아니면 직접 실행
		if (locConf != NULL && !locConf->opCgiPassDirective.empty()) {
			return locConf->opCgiPassDirective[0].path;
		}
		return ""; // 실행 파일로 간주
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

	// 알 수 없는 확장자: cgi_pass 사용
	if (locConf != NULL && !locConf->opCgiPassDirective.empty()) {
		return locConf->opCgiPassDirective[0].path;
	}

	return ""; // cgi_pass도 없으면 직접 실행으로 간주
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

	// Chunked 인코딩의 경우 Content-Length 헤더가 없으므로, 실제 body 크기를 사용
    // 4. CONTENT_LENGTH (🔥 Zero-Copy 지원)
    size_t contentLength = _request->getBodyLength();  // getBody() → getBodyLength()
    DEBUG_LOG("[CgiExecutor] Setting CONTENT_LENGTH=" << contentLength);
    
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
	// PHP-CGI는 SCRIPT_NAME, SCRIPT_FILENAME을 필수로 필요로 함
	// ubuntu_cgi_tester는 이 환경변수들이 없어야 PATH_INFO만으로 작동함

	// 파일명 추출
	size_t lastSlash = _cgiPath.find_last_of('/');
	std::string fileName = (lastSlash != std::string::npos) ?
		_cgiPath.substr(lastSlash + 1) : _cgiPath;

	// 파일명의 끝이 .php인지 확인 (경로에 .php가 포함되어 있어도 무시)
	bool isPhpCgi = (fileName.length() >= 4 &&
		fileName.substr(fileName.length() - 4) == ".php");

	if (isPhpCgi) {
		std::string scriptName = uri;
		size_t scriptQPos = scriptName.find('?');
		if (scriptQPos != std::string::npos) {
			scriptName = scriptName.substr(0, scriptQPos);
		}
		envList.push_back("SCRIPT_NAME=" + scriptName);
		envList.push_back("SCRIPT_FILENAME=" + _cgiPath);
	}

	if (!_locConf->opRootDirective.empty()) {
		envList.push_back("DOCUMENT_ROOT=" + _locConf->opRootDirective[0].path);
	} else if (!_serverConf->opRootDirective.empty()) {
		envList.push_back("DOCUMENT_ROOT=" + _serverConf->opRootDirective[0].path);
	}

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

// src/cgi/CgiExecutor.cpp (execute 메서드만 수정)
// CgiExecutor.cpp - execute() 수정
std::string CgiExecutor::execute() {
    std::string interpreter = getInterpreter(_cgiPath, _locConf);
    
    // Pipe 생성
    int pipeStdin[2];
    int pipeStdout[2];
    int pipeStderr[2];
    
    if (pipe(pipeStdin) == -1 || pipe(pipeStdout) == -1 || pipe(pipeStderr) == -1) {
        return "";
    }
    
    // ========== 🔥 Body를 임시 파일로 저장 (핵심 최적화!) ==========
    int tmpBodyFd = -1;
    char tmpBodyPath[] = "/tmp/cgi_body_XXXXXX";
    
    size_t bodyLength = _request->getBodyLength();
    if (bodyLength > 0) {
        // 임시 파일 생성
        tmpBodyFd = mkstemp(tmpBodyPath);
        if (tmpBodyFd == -1) {
            close(pipeStdin[0]); close(pipeStdin[1]);
            close(pipeStdout[0]); close(pipeStdout[1]);
            close(pipeStderr[0]); close(pipeStderr[1]);
            return "";
        }
        
        // Body 데이터를 파일에 쓰기
        const char* bodyData = _request->getBodyData();
        size_t written = 0;
        while (written < bodyLength) {
            ssize_t w = write(tmpBodyFd, bodyData + written, bodyLength - written);
            if (w <= 0) break;
            written += w;
        }

        // 파일 포인터를 처음으로 되돌리기
        lseek(tmpBodyFd, 0, SEEK_SET);
    }
    
    // Fork
    pid_t pid = fork();
    if (pid == -1) {
        if (tmpBodyFd != -1) {
            close(tmpBodyFd);
            unlink(tmpBodyPath);
        }
        close(pipeStdin[0]); close(pipeStdin[1]);
        close(pipeStdout[0]); close(pipeStdout[1]);
        close(pipeStderr[0]); close(pipeStderr[1]);
        return "";
    }
    
    if (pid == 0) {
        // ========== 자식 프로세스 ==========
        
        // ✅ 임시 파일을 stdin으로 리다이렉트
        if (tmpBodyFd != -1) {
            dup2(tmpBodyFd, STDIN_FILENO);
            close(tmpBodyFd);
        } else {
            // Body 없으면 /dev/null
            int devnull = open("/dev/null", O_RDONLY);
            dup2(devnull, STDIN_FILENO);
            close(devnull);
        }
        
        // Stdout, Stderr 리다이렉트
        dup2(pipeStdout[1], STDOUT_FILENO);
        dup2(pipeStderr[1], STDERR_FILENO);
        
        close(pipeStdin[0]); close(pipeStdin[1]);
        close(pipeStdout[0]); close(pipeStdout[1]);
        close(pipeStderr[0]); close(pipeStderr[1]);
        
        // Working directory 변경
        std::string scriptDir = getDirectoryFromPath(_cgiPath);
        chdir(scriptDir.c_str());

        // Execve
        char* argv[3];

        if (!interpreter.empty()) {
            argv[0] = stringDup(interpreter);
            argv[1] = stringDup(_cgiPath);
            argv[2] = NULL;
            execve(interpreter.c_str(), argv, _envp);
        } else {
            argv[0] = stringDup(_cgiPath);
            argv[1] = NULL;
            execve(_cgiPath.c_str(), argv, _envp);
        }

        exit(1);
    }
    
    // ========== 부모 프로세스 ==========
    
    // ✅ 임시 파일 정리 (자식이 이미 열었으므로 부모는 닫아도 됨)
    if (tmpBodyFd != -1) {
        close(tmpBodyFd);
        unlink(tmpBodyPath);  // 파일 삭제 (자식은 fd로 여전히 접근 가능)
    }
    
    // Stdin pipe는 이제 필요 없음 (파일 사용)
    close(pipeStdin[0]);
    close(pipeStdin[1]);
    
    close(pipeStdout[1]);
    close(pipeStderr[1]);
    
    fcntl(pipeStdout[0], F_SETFL, O_NONBLOCK);
    fcntl(pipeStderr[0], F_SETFL, O_NONBLOCK);
    
    std::string output;
    std::string errorOutput;
    char buffer[65536];
    
    int stdoutFd = pipeStdout[0];
    int stderrFd = pipeStderr[0];
    
    // ✅ Stdout/Stderr만 읽기 (stdin write 필요 없음!)
    while (stdoutFd != -1 || stderrFd != -1) {
        fd_set readFds;
        FD_ZERO(&readFds);
        int maxFd = -1;
        
        if (stdoutFd != -1) {
            FD_SET(stdoutFd, &readFds);
            maxFd = std::max(maxFd, stdoutFd);
        }
        
        if (stderrFd != -1) {
            FD_SET(stderrFd, &readFds);
            maxFd = std::max(maxFd, stderrFd);
        }
        
        if (maxFd == -1) break;
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ready = select(maxFd + 1, &readFds, NULL, NULL, &timeout);
        
        if (ready < 0) break;
        
        // Stdout 읽기
        if (stdoutFd != -1 && FD_ISSET(stdoutFd, &readFds)) {
            ssize_t bytesRead = read(stdoutFd, buffer, sizeof(buffer));
            if (bytesRead > 0) {
                output.append(buffer, bytesRead);
            } else if (bytesRead == 0) {
                close(stdoutFd);
                stdoutFd = -1;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                close(stdoutFd);
                stdoutFd = -1;
            }
        }
        
        // Stderr 읽기
        if (stderrFd != -1 && FD_ISSET(stderrFd, &readFds)) {
            ssize_t bytesRead = read(stderrFd, buffer, sizeof(buffer));
            if (bytesRead > 0) {
                errorOutput.append(buffer, bytesRead);
            } else if (bytesRead == 0) {
                close(stderrFd);
                stderrFd = -1;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                close(stderrFd);
                stderrFd = -1;
            }
        }
    }
    
    if (stdoutFd != -1) close(stdoutFd);
    if (stderrFd != -1) close(stderrFd);
    
    // 자식 프로세스 종료 대기
    int status;
    waitpid(pid, &status, 0);

    // ✅ 출력이 있으면 성공 (PHP는 정상 동작해도 non-zero exit code 반환 가능)
    if (!output.empty()) {
        return output;
    }

    // 출력이 없으면 exit code 확인
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return "";
    }

    return output;
}

