#include "http/HttpResponse.hpp"
#include "http/HttpRequest.hpp"
#include "http/StatusCode.hpp"
#include "config/ConfigManager.hpp"
#include "utils/FileManager.hpp"
#include <sstream>
#include <fstream>
#include <ctime>

// ============ 생성자와 소멸자 ============
HttpResponse::HttpResponse() : _statusCode(StatusCode::OK) {}

HttpResponse::~HttpResponse() {}

// ============ Setter 함수들 ============
void HttpResponse::setStatus(int code) {
	_statusCode = code;
}

void HttpResponse::setHeader(const std::string& key, const std::string& value) {
	_headers[key] = value;
}

void HttpResponse::setBody(const std::string& body) {
	_body = body;
}

void HttpResponse::setContentType(const std::string& type) {
	setHeader("Content-Type", type);
}

// ============ 응답 생성 ============
std::string HttpResponse::serialize(const HttpRequest* request) {
	std::stringstream ss;
	
	// 1. Status Line (StatusCode에서 메시지 가져오기)
	ss << "HTTP/1.1 " << _statusCode << " " << StatusCode::getReasonPhrase(_statusCode) << "\r\n";
	
	// 2. 기본 헤더 설정 (Date, Server, Connection)
	setDefaultHeaders(request);
	
	// 3. Content-Length 자동 계산
	if (_headers.find("Content-Length") == _headers.end()) {
		std::stringstream len_ss;
		len_ss << _body.length();
		_headers["Content-Length"] = len_ss.str();
	}
	
	// 4. 모든 헤더 추가
	for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
		ss << it->first << ": " << it->second << "\r\n";
	}
	
	// 5. 헤더와 바디 구분
	ss << "\r\n";
	
	// 6. 바디 추가 (GET, HEAD 메서드는 바디가 없을 수 있음)
	if (request == NULL || (request->getMethod() != "HEAD")) {
		ss << _body;
	}
	
	return ss.str();
}

// ============ 에러 응답 생성 (모든 로직 중앙화) ============
HttpResponse HttpResponse::createErrorResponse(int code, const ServerContext* serverConf, const LocationContext* locConf) {
	HttpResponse response;
	response.setStatus(code);
	
	std::string errorBody;

	if (serverConf != NULL) {
		// 1. 커스텀 에러 페이지 경로를 조회
		std::string customErrorPagePath = ConfigManager::findErrorPagePath(code, serverConf, locConf);

		// 2. 경로를 찾았다면, 해당 파일을 로드한다
		if (!customErrorPagePath.empty()) {
			DEEP_LOG("[HttpResponse] Attempting to load custom error page: " << customErrorPagePath);
			
			if (!FileManager::readFile(customErrorPagePath, errorBody)) {
				// 커스텀 에러 페이지 로드 실패
				ERROR_LOG("[HttpResponse] Failed to load custom error page: " << customErrorPagePath);
				errorBody.clear();  // 실패했으니 비우기
			} else {
				DEBUG_LOG("[HttpResponse] Custom error page loaded successfully: " << customErrorPagePath);
			}
		} else {
			DEEP_LOG("[HttpResponse] No custom error page configured for code " << code);
		}
	}

	// 3. 커스텀 페이지가 없거나 로드에 실패했다면, 하드코딩된 기본 페이지를 생성
	if (errorBody.empty()) {
		DEBUG_LOG("[HttpResponse] Using default error page for code " << code);
		std::stringstream html;
		html << "<html><body><h1>" << code << " " << StatusCode::getReasonPhrase(code) << "</h1></body></html>";
		errorBody = html.str();
	}
	
	response.setBody(errorBody);
	response.setContentType("text/html; charset=utf-8");
	
	return response;
}



// ============ 내부 유틸리티 ============
void HttpResponse::setDefaultHeaders(const HttpRequest* request) {
	// Date 헤더 (RFC 1123 형식)
	if (_headers.find("Date") == _headers.end()) {
		char buf[100];
		time_t now = time(0);
		struct tm* tm = gmtime(&now);
		strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", tm);
		_headers["Date"] = buf;
	}
	
	// Server 헤더
	if (_headers.find("Server") == _headers.end()) {
		_headers["Server"] = "webserv/1.0";
	}
	
	// Connection 헤더 (HttpRequest 기반으로 동적 설정)
	if (_headers.find("Connection") == _headers.end()) {
		if (request && request->isKeepAlive()) {
			_headers["Connection"] = "keep-alive";
		} else {
			_headers["Connection"] = "close";
		}
	}
}
