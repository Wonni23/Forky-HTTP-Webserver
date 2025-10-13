#include "http/HttpController.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/StatusCode.hpp"
#include "http/RequestRouter.hpp" // 설정 찾기
#include "utils/FileUtils.hpp"	// 파일 시스템 관련 유틸리티
#include "utils/StringUtils.hpp"
#include "utils/PathResolver.hpp"
#include "utils/FileManager.hpp"
#include "cgi/CgiExecuter.hpp"
#include "cgi/CgiResponse.hpp"

// =========================================================================
// Public Interface
// =========================================================================

HttpResponse* HttpController::processRequest(const HttpRequest* request, int connectedPort) {
	// 1. 요청에 맞는 설정 찾기
	const ServerContext* serverConf = RequestRouter::findServerForRequest(request, connectedPort);
	if (!serverConf) {
		ERROR_LOG("No matching server config found.");
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, NULL, NULL));
	}
	const LocationContext* locConf = RequestRouter::findLocationForRequest(serverConf, request->getUri());
	if (!locConf) {
		ERROR_LOG("No matching location config found for URI: " << request->getUri());
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf));
	}

	// 2. 리다이렉션 규칙 확인 (return 지시어)
	if (!locConf->opReturnDirective.empty()) {
		return handleRedirect(locConf);
	}
 
	// 3. 허용된 메서드인지 확인 (limit_except 지시어)
	if (!isMethodAllowed(request->getMethod(), locConf)) {
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::METHOD_NOT_ALLOWED, serverConf, locConf));
	}

	// 4. CGI 실행 여부 확인
	// std::string cgiPath = getCgiPath(request, serverConf, locConf);
	// if (!cgiPath.empty()) {
	// 	return executeCgi(request, cgiPath, resourcePath);
	// }  // 일단 주석처리

	// 5. HTTP 메서드에 따라 분기
	const std::string& method = request->getMethod();
	if (method == "GET" || method == "HEAD") {
		return handleGetRequest(request, serverConf, locConf);
	} else if (method == "POST") {
		return handlePostRequest(request, serverConf, locConf);
	} else if (method == "DELETE") {
		return handleDeleteRequest(request, serverConf, locConf);
	}

	// 지원하지 않는 메서드
	return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_IMPLEMENTED, serverConf, locConf));
}

// =========================================================================
// Private Method Handlers
// =========================================================================

HttpResponse* HttpController::handleGetRequest(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf) {
	std::string resourcePath = PathResolver::resolvePath(serverConf, locConf, request->getUri());

	if (!FileUtils::pathExists(resourcePath)) {
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf));
	}

	if (FileUtils::isDirectory(resourcePath)) {
		// 1. 디렉토리일 경우: index 파일 시도
		std::string indexPath = PathResolver::findIndexFile(resourcePath, locConf);
		if (!indexPath.empty()) {
			return serveStaticFile(indexPath, locConf);
		}
		
		// 2. index 파일이 없고, 디렉토리 리스팅이 허용된 경우
		if (locConf->opAutoindexDirective[0].enabled) {
			return serveDirectoryListing(resourcePath, request->getUri());
		}
		
		// 3. 디렉토리 리스팅이 금지된 경우
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::FORBIDDEN, serverConf, locConf));
	}

	// 일반 파일 서빙
	return serveStaticFile(resourcePath, locConf);
}

HttpResponse* HttpController::handlePostRequest(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf) {
	// POST 요청은 주로 파일 업로드나 CGI 처리에 사용됨
	// 파일 업로드 경로가 설정되어 있는지 확인
	if (locConf->opRootDirective.empty() || locConf->opRootDirective[0].path.empty()) {
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::FORBIDDEN, serverConf, locConf));
	}

	// 바디 크기 제한 확인
	if (request->getBody().length() > StringUtils::toBytes(locConf->opBodySizeDirective[0].size)) {
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::PAYLOAD_TOO_LARGE, serverConf, locConf));
	}

	// 파일 생성
	std::string filePath = FileManager::generateUploadFilePath(locConf->opRootDirective[0].path);
	if (!FileManager::saveFile(filePath, request->getBody())) {
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf));
	}
	
	// 201 Created 응답 생성
	HttpResponse* response = new HttpResponse();
	response->setStatus(StatusCode::CREATED);
	response->setHeader("Location", filePath); // 생성된 리소스의 위치
	response->setBody("<html><body><h1>201 Created</h1><p>File uploaded successfully to " + filePath + "</p></body></html>");
	response->setContentType("text/html; charset=utf-8");
	return response;
}

HttpResponse* HttpController::handleDeleteRequest(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf) {
	std::string resourcePath = PathResolver::resolvePath(serverConf, locConf, request->getUri());

	if (!FileUtils::pathExists(resourcePath)) {
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf));
	}
	if (FileUtils::isDirectory(resourcePath)) {
		// 디렉토리 삭제는 허용하지 않음 (안전성)
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::FORBIDDEN, serverConf, locConf));
	}

	if (!FileManager::deleteFile(resourcePath)) {
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf));
	}

	// 200 OK 또는 204 No Content 응답 생성
	HttpResponse* response = new HttpResponse();
	response->setStatus(StatusCode::OK);
	response->setBody("<html><body><h1>200 OK</h1><p>File deleted successfully.</p></body></html>");
	response->setContentType("text/html; charset=utf-8");
	return response;
}

// =========================================================================
// Private Helper Functions
// =========================================================================

bool HttpController::isMethodAllowed(const std::string& method, const LocationContext* locConf) {
	// 1. If there is no limit_except directive -> allow all methods
	if (locConf->opLimitExceptDirective.empty())
		return true;

	// Using first element of the vector
	const LimitExceptDirective& directive = locConf->opLimitExceptDirective[0];
	const std::set<std::string>& allowed = directive.allowed_methods;

	// 2. Check if there is current method in allowed methods
	if (allowed.find(method) != allowed.end())
		return true;

	// 3. If it's not in allowed method set, decide by deny_all setting
	if (directive.deny_all)
		return false;
		
	// 4. If it's not in allowed method set && deny_all is not setted
	return false;
}

HttpResponse* HttpController::handleRedirect(const LocationContext* locConf) {
	const ReturnDirective& redirect = locConf->opReturnDirective[0];
	HttpResponse* response = new HttpResponse();
	response->setStatus(redirect.code);
	response->setHeader("Location", redirect.url);
	response->setBody("<html><body>Redirecting...</body></html>");
	response->setContentType("text/html; charset=utf-8");
	return response;
}

HttpResponse* HttpController::serveStaticFile(const std::string& filePath, const LocationContext* locConf) {
	(void)locConf; // 현재는 사용하지 않음
	std::string content = FileManager::readFile(filePath);
	
	HttpResponse* response = new HttpResponse();
	response->setStatus(StatusCode::OK);
	response->setBody(content);
	response->setContentType(FileUtils::getMimeType(filePath));
	return response;
}

HttpResponse* HttpController::serveDirectoryListing(const std::string& dirPath, const std::string& uri) {
	std::string html = FileManager::generateDirectoryListing(dirPath, uri);
	
	HttpResponse* response = new HttpResponse();
	response->setStatus(StatusCode::OK);
	response->setBody(html);
	response->setContentType("text/html; charset=utf-8");
	return response;
}

// std::string	HttpController::getCgiPath(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf) {
// 	// 1. 이 location에 cgi_pass가 설정되어 있는가?
// 	if (locConf->opCgiPassDirective.empty()) {
// 		return ""; // CGI 설정 없음 → 일반 요청
// 	}

// 	// 2. 요청 경로를 실제 파일 시스템 경로로 변환.
// 	std::string resourcePath = PathResolver::resolvePath(serverConf, locConf, request->getUri());
// 	if (resourcePath.empty()) {
// 		return ""; // 경로 해석 실패
// 	}

// 	// 3. 해당 파일이 실제로 존재하고, '파일'이 맞는가?
// 	if (FileUtils::pathExists(resourcePath) && !FileUtils::isDirectory(resourcePath)) {
// 		// CGI 스크립트 파일 발견! 경로 반환.
// 		return resourcePath;
// 	}

// 	// 모든 조건을 만족하지 못하면 CGI 요청이 아님.
// 	return "";
// }

// HttpResponse* HttpController::executeCgi(const HttpRequest* request, const std::string& cgiPath, const ServerContext* serverConf, const LocationContext* locConf) {
// 	// 1단계. CGI 실행 객체 실행
// 	CgiExecutor executor(request, cgiPath, serverConf, locConf);
// 	std::string cgiOutput = executor.execute(); // 여기서 fork, exec, pipe 등 모든 일이 일어남

// 	// 실행 자체가 실패했는가? (스크립트 없음, 권한 없음 등)
// 	if (cgiOutput.empty()) {
// 		ERROR_LOG("CGI execution failed for path: " + cgiPath);
// 		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf));
// 	}

// 	// 2. CGI 응답 해석
// 	CgiResponseParser parser;
// 	HttpResponse* response = parser.parse(cgiOutput);

// 	// 파싱 자체가 실패했는가? (CGI가 엉터리 응답을 뱉음)
// 	if (!response) {
// 		ERROR_LOG("Failed to parse CGI output from: " + cgiPath);
// 		// CGI 스크립트가 잘못된 응답을 보냈으므로, '502 Bad Gateway'가 더 정확함
// 		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::BAD_GATEWAY, serverConf, locConf));
// 	}

// 	// 3. HttpResponse를 수령하여 반환
// 	return response;
// }

