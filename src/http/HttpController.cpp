#include "HttpController.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "StatusCode.hpp"
#include "RequestRouter.hpp" // 설정 찾기
#include "utils/FileUtils.hpp"     // 파일 시스템 관련 유틸리티
#include "utils/StringUtils.hpp"

// =========================================================================
// Public Interface
// =========================================================================

HttpResponse* HttpController::processRequest(const HttpRequest* request, int connectedPort) {
	// 1. 요청에 맞는 설정 찾기
	const ServerContext* serverConf = RequestRouter::findServerForRequest(request, connectedPort);
	if (!serverConf) {
		ERROR_LOG("No matching server config found.");
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, ""));
	}
	const LocationContext* locConf = RequestRouter::findLocationForRequest(serverConf, request->getUri());
	if (!locConf) {
		ERROR_LOG("No matching location config found for URI: " << request->getUri());
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf->error_page_path));
	}

	// 2. 리다이렉션 규칙 확인 (return 지시어)
	if (!locConf->opReturnDirective.empty()) {
		return handleRedirect(locConf);
	}

	// 3. 허용된 메서드인지 확인 (limit_except 지시어)
	if (!isMethodAllowed(request->getMethod(), locConf)) {
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::METHOD_NOT_ALLOWED, serverConf->error_page_path));
	}

	// 4. CGI 실행 여부 확인
	std::string cgiPath = FileUtils::getCgiPath(request->getUri(), locConf);
	if (!cgiPath.empty()) {
		std::string resourcePath = FileUtils::resolvePath(serverConf, locConf, request->getUri());
		return executeCgi(request, cgiPath, resourcePath);
	}

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
	return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_IMPLEMENTED, serverConf->error_page_path));
}

// =========================================================================
// Private Method Handlers
// =========================================================================

HttpResponse* HttpController::handleGetRequest(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf) {
	std::string resourcePath = FileUtils::resolvePath(serverConf, locConf, request->getUri());

	if (!FileUtils::pathExists(resourcePath)) {
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf->error_page_path));
	}

	if (FileUtils::isDirectory(resourcePath)) {
		// 1. 디렉토리일 경우: index 파일 시도
		std::string indexPath = FileUtils::findIndexFile(resourcePath, locConf);
		if (!indexPath.empty()) {
			return serveStaticFile(indexPath, locConf);
		}
		
		// 2. index 파일이 없고, 디렉토리 리스팅이 허용된 경우
		if (locConf->opAutoindexDirective[0].enabled) {
			return serveDirectoryListing(resourcePath, request->getUri());
		}
		
		// 3. 디렉토리 리스팅이 금지된 경우
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::FORBIDDEN, serverConf->error_page_path));
	}

	// 일반 파일 서빙
	return serveStaticFile(resourcePath, locConf);
}

HttpResponse* HttpController::handlePostRequest(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf) {
	// POST 요청은 주로 파일 업로드나 CGI 처리에 사용됨
	// 파일 업로드 경로가 설정되어 있는지 확인
	if (locConf->opUploadStoreDirective[0].path.empty()) {
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::FORBIDDEN, serverConf->error_page_path));
	}

	// 바디 크기 제한 확인
	if (request->getBody().length() > StringUtils::toBytes(locConf->opBodySizeDirective[0].size)) {
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::PAYLOAD_TOO_LARGE, serverConf->error_page_path));
	}

	// 파일 생성 (파일 이름은 예시로 현재 시간 사용)
	std::string filePath = FileUtils::generateUploadFilePath(locConf->opUploadStoreDirective.path);
	if (!FileUtils::saveFile(filePath, request->getBody())) {
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf->error_page_path));
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
	std::string resourcePath = FileUtils::resolvePath(serverConf, locConf, request->getUri());

	if (!FileUtils::pathExists(resourcePath)) {
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf->error_page_path));
	}
	if (FileUtils::isDirectory(resourcePath)) {
		// 디렉토리 삭제는 허용하지 않음 (안전성)
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::FORBIDDEN, serverConf->error_page_path));
	}

	if (!FileUtils::deleteFile(resourcePath)) {
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf->error_page_path));
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
	std::string content = FileUtils::readFile(filePath);
	
	HttpResponse* response = new HttpResponse();
	response->setStatus(StatusCode::OK);
	response->setBody(content);
	response->setContentType(FileUtils::getMimeType(filePath));
	return response;
}

HttpResponse* HttpController::serveDirectoryListing(const std::string& dirPath, const std::string& uri) {
	std::string html = FileUtils::generateDirectoryListing(dirPath, uri);
	
	HttpResponse* response = new HttpResponse();
	response->setStatus(StatusCode::OK);
	response->setBody(html);
	response->setContentType("text/html; charset=utf-8");
	return response;
}

HttpResponse* HttpController::executeCgi(const HttpRequest* request, const std::string& cgiPath, const std::string& resourcePath) {
	// CGIExecutor 클래스가 있다고 가정하고 위임
	// CgiExecutor cgiExecutor(request, cgiPath, resourcePath);
	// std::string cgiOutput = cgiExecutor.execute();

	// 임시 CGI 응답
	(void)request; (void)cgiPath; (void)resourcePath;
	std::string cgiOutput = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello from CGI!";
	
	// CGI 출력 파싱하여 HttpResponse 객체 생성 필요 (별도 클래스 권장)
	// CgiResponseParser parser;
	// return parser.parse(cgiOutput);

	HttpResponse* response = new HttpResponse();
	response->setStatus(StatusCode::OK);
	response->setBody("Hello from CGI!");
	response->setContentType("text/plain");
	return response;
}

