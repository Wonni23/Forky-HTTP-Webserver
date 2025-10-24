#include "http/HttpController.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/StatusCode.hpp"
#include "http/RequestRouter.hpp"
#include "utils/FileUtils.hpp"
#include "utils/StringUtils.hpp"
#include "utils/PathResolver.hpp"
#include "utils/FileManager.hpp"
#include "cgi/CgiExecuter.hpp"
#include "cgi/CgiResponse.hpp"


// =========================================================================
// Public Interface
// =========================================================================


HttpResponse* HttpController::processRequest(const HttpRequest* request, int connectedPort) {
	DEBUG_LOG("[HttpController] ===== Processing HTTP request =====");
	DEBUG_LOG("[HttpController] Method: " << request->getMethod() << " URI: " << request->getUri() << " Port: " << connectedPort);
	
	// 1. 요청에 맞는 설정 찾기
	const ServerContext* serverConf = RequestRouter::findServerForRequest(request, connectedPort);
	if (!serverConf) {
		ERROR_LOG("[HttpController] No matching server config found for port=" << connectedPort);
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, NULL, NULL));
	}
	
	std::string serverName = serverConf->opServerNameDirective.empty() ? "(unnamed)" : serverConf->opServerNameDirective[0].name;
	DEEP_LOG("[HttpController] Server matched: " << serverName);
	
	const LocationContext* locConf = RequestRouter::findLocationForRequest(serverConf, request->getUri());
	if (!locConf) {
		ERROR_LOG("[HttpController] No matching location config found for URI: " << request->getUri());
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf));
	}
	
	DEEP_LOG("[HttpController] Location matched: " << locConf->path);

	// 2. 리다이렉션 규칙 확인 (return 지시어)
	if (!locConf->opReturnDirective.empty()) {
		DEBUG_LOG("[HttpController] Redirect directive found: code=" << locConf->opReturnDirective[0].code 
				  << " url=" << locConf->opReturnDirective[0].url);
		return handleRedirect(locConf);
	}
 
	// 3. 허용된 메서드인지 확인 (limit_except 지시어)
	if (!isMethodAllowed(request->getMethod(), locConf) || request->getMethod() == "HEAD") {
		ERROR_LOG("[HttpController] Method not allowed: " << request->getMethod() << " for location: " << locConf->path);
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::METHOD_NOT_ALLOWED, serverConf, locConf));
	}
	
	DEEP_LOG("[HttpController] Method allowed: " << request->getMethod());

	// 4. CGI 실행 여부 확인 (TODO: 리팩토링 시 CgiService로 이동 고려)
	std::string cgiPath = getCgiPath(request, serverConf, locConf);
	if (!cgiPath.empty()) {
		DEBUG_LOG("[HttpController] CGI execution path: " << cgiPath);
		return executeCgi(request, cgiPath, serverConf, locConf);
	}

	// 4-1. CGI location인데 파일이 없으면 404 (파일 업로드로 오인 방지)
	if (!locConf->opCgiPassDirective.empty()) {
		// cgi_pass가 설정되어 있지만 getCgiPath()가 빈 문자열 = 파일 없음
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf));
	}

	// 5. HTTP 메서드에 따라 분기
	const std::string& method = request->getMethod();
	if (method == "GET" || method == "HEAD") {
		DEBUG_LOG("[HttpController] Dispatching to GET handler");
		return handleGetRequest(request, serverConf, locConf);
	} else if (method == "POST") {
		DEBUG_LOG("[HttpController] Dispatching to POST handler");
		return handlePostRequest(request, serverConf, locConf);
	} else if (method == "DELETE") {
		DEBUG_LOG("[HttpController] Dispatching to DELETE handler");
		return handleDeleteRequest(request, serverConf, locConf);
	}

	// 지원하지 않는 메서드
	ERROR_LOG("[HttpController] Unsupported method: " << method);
	return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_IMPLEMENTED, serverConf, locConf));
}


// =========================================================================
// Private Method Handlers
// =========================================================================


HttpResponse* HttpController::handleGetRequest(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf) {
	// NULL Guards
	if (!request) {
		ERROR_LOG("[HttpController] Request is NULL");
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, NULL, NULL));
	}
	if (!serverConf) {
		ERROR_LOG("[HttpController] ServerContext is NULL");
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, NULL, NULL));
	}
	if (!locConf) {
		ERROR_LOG("[HttpController] LocationContext is NULL");
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, NULL));
	}

	DEBUG_LOG("[HttpController] ===== Handling GET request =====");
	
	std::string resourcePath = PathResolver::resolvePath(serverConf, locConf, request->getUri());
	DEBUG_LOG("[HttpController] Resolved path: " << resourcePath);

	if (!FileUtils::pathExists(resourcePath)) {
		ERROR_LOG("[HttpController] Path not found: " << resourcePath);
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf));
	}

	if (FileUtils::isDirectory(resourcePath)) {
		DEBUG_LOG("[HttpController] Path is directory: " << resourcePath);
		
		// 1. 디렉토리일 경우: index 파일 시도
		std::string indexPath = PathResolver::findIndexFile(resourcePath, locConf);
		if (!indexPath.empty()) {
			DEBUG_LOG("[HttpController] Index file found: " << indexPath);
			return serveStaticFile(indexPath, locConf);
		}
		
		DEEP_LOG("[HttpController] No index file found");
		
		// 2. index 파일이 없고, 디렉토리 리스팅이 허용된 경우
		if (!locConf->opAutoindexDirective.empty() &&
			locConf->opAutoindexDirective[0].enabled) {
			DEBUG_LOG("[HttpController] Serving directory listing (autoindex enabled)");
			return serveDirectoryListing(resourcePath, request->getUri());
		}
		
		// 3. 디렉토리 리스팅이 금지된 경우
		ERROR_LOG("[HttpController] Directory listing forbidden for: " << resourcePath);
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf));
	}

	// 일반 파일 서빙
	DEBUG_LOG("[HttpController] Serving static file: " << resourcePath);
	return serveStaticFile(resourcePath, locConf);
}


HttpResponse* HttpController::handlePostRequest(
	const HttpRequest* request, 
	const ServerContext* serverConf, 
	const LocationContext* locConf) 
{
	// NULL 체크
	if (!request || !serverConf || !locConf) {
		ERROR_LOG("[HttpController] NULL parameter in POST handler");
		return new HttpResponse(HttpResponse::createErrorResponse(
			StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf));
	}
	
	DEBUG_LOG("[HttpController] ===== Handling POST request =====");
	DEBUG_LOG("[HttpController] Body size: " << request->getBody().length() << " bytes");
	
	// PathResolver
	std::string uploadRoot = PathResolver::resolvePath(serverConf, locConf, request->getUri());
	
	if (uploadRoot.empty()) {
		ERROR_LOG("[HttpController] Failed to resolve upload path");
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::FORBIDDEN, serverConf, locConf));
	}
	
	DEEP_LOG("[HttpController] Upload root: " << uploadRoot);
	
	// Body size 제한
	size_t maxBodySize = locConf->opBodySizeDirective.empty() ? 1024 * 1024 : StringUtils::toBytes(locConf->opBodySizeDirective[0].size);
	DEEP_LOG("[HttpController] Max body size: " << maxBodySize << " bytes");
	
	if (request->getBody().length() > maxBodySize) {
		ERROR_LOG("[HttpController] Body too large: " << request->getBody().length() << " bytes (max: " << maxBodySize << ")");
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::PAYLOAD_TOO_LARGE, serverConf, locConf));
	}
	
	// 파일 생성
	std::string filePath = FileManager::generateUploadFilePath(uploadRoot);
	DEBUG_LOG("[HttpController] Generated upload path: " << filePath);
	
	if (!FileManager::saveFile(filePath, request->getBody())) {
		ERROR_LOG("[HttpController] Failed to save file: " << filePath);
		return new HttpResponse(HttpResponse::createErrorResponse(
			StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf));
	}
	
	DEBUG_LOG("[HttpController] File uploaded successfully: " << filePath);
	
	// 201 Created 응답
	HttpResponse* response = new HttpResponse();
	response->setStatus(StatusCode::CREATED);
	response->setHeader("Location", filePath);
	response->setBody("<html><body><h1>201 Created</h1>"
					 "<p>File uploaded successfully to " + filePath + "</p>"
					 "</body></html>");
	response->setContentType("text/html; charset=utf-8");
	return response;
}



HttpResponse* HttpController::handleDeleteRequest(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf) {
	if (!request || !serverConf || !locConf) {
		ERROR_LOG("[HttpController] NULL parameter in POST handler");
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf));
	}
	
	DEBUG_LOG("[HttpController] ===== Handling DELETE request =====");
	
	std::string resourcePath = PathResolver::resolvePath(serverConf, locConf, request->getUri());
	DEBUG_LOG("[HttpController] Resolved path: " << resourcePath);

	if (!FileUtils::pathExists(resourcePath)) {
		ERROR_LOG("[HttpController] Path not found: " << resourcePath);
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf));
	}
	
	if (FileUtils::isDirectory(resourcePath)) {
		// 디렉토리 삭제는 허용하지 않음 (안전성)
		ERROR_LOG("[HttpController] Cannot delete directory: " << resourcePath);
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::FORBIDDEN, serverConf, locConf));
	}

	DEEP_LOG("[HttpController] Attempting to delete file: " << resourcePath);
	
	if (!FileManager::deleteFile(resourcePath)) {
		ERROR_LOG("[HttpController] Failed to delete file: " << resourcePath);
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf));
	}

	DEBUG_LOG("[HttpController] File deleted successfully: " << resourcePath);

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
	if (!locConf) {
		ERROR_LOG("[HttpController] LocationContext is NULL in isMethodAllowed");
		return false;  // 안전하게 거부
	}
	
	DEEP_LOG("[HttpController] Checking if method allowed: " << method);
	
	// 1. If there is no limit_except directive -> allow all methods
	if (locConf->opLimitExceptDirective.empty()) {
		DEEP_LOG("[HttpController] No limit_except directive, allowing all methods");
		return true;
	}

	// Using first element of the vector
	const LimitExceptDirective& directive = locConf->opLimitExceptDirective[0];
	const std::set<std::string>& allowed = directive.allowed_methods;

	DEEP_LOG("[HttpController] limit_except configured, deny_all=" << directive.deny_all);

	// 2. Check if there is current method in allowed methods
	if (allowed.find(method) != allowed.end()) {
		DEEP_LOG("[HttpController] Method " << method << " is in allowed list");
		return true;
	}

	DEEP_LOG("[HttpController] Method " << method << " not in allowed list");

	// 3. If it's not in allowed method set, decide by deny_all setting
	if (directive.deny_all) {
		DEEP_LOG("[HttpController] deny_all is set, rejecting method");
		return false;
	}
		
	// 4. If it's not in allowed method set && deny_all is not setted
	return false;
}


HttpResponse* HttpController::handleRedirect(const LocationContext* locConf) {
	const ReturnDirective& redirect = locConf->opReturnDirective[0];
	
	DEBUG_LOG("[HttpController] Creating redirect response: code=" << redirect.code << " url=" << redirect.url);
	
	HttpResponse* response = new HttpResponse();
	response->setStatus(redirect.code);
	response->setHeader("Location", redirect.url);
	response->setBody("<html><body>Redirecting...</body></html>");
	response->setContentType("text/html; charset=utf-8");
	return response;
}


HttpResponse* HttpController::serveStaticFile(const std::string& filePath, const LocationContext* locConf) {
	(void)locConf;
	
	DEBUG_LOG("[HttpController] Reading static file: " << filePath);
	
	std::string content;
	
	if (!FileManager::readFile(filePath, content)) {
		ERROR_LOG("[HttpController] Failed to read file: " << filePath);
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, NULL, locConf));
	}
	
	std::string mimeType = FileUtils::getMimeTypeFromPath(filePath);
	DEEP_LOG("[HttpController] File size: " << content.length() << " bytes, MIME type: " << mimeType);
	
	HttpResponse* response = new HttpResponse();
	response->setStatus(StatusCode::OK);
	response->setBody(content);
	response->setContentType(mimeType);

	// 브라우저에서 렌더링 가능한 타입은 inline
	if (mimeType.find("text/") == 0 ||
		mimeType.find("image/") == 0 ||
		mimeType == "application/pdf" ||
		mimeType == "application/json" ||
		mimeType == "application/javascript") {
		
		response->setHeader("Content-Disposition", "inline");
	} else {
		// 다운로드 필요한 파일 (zip 등)
		size_t pos = filePath.find_last_of('/');
		std::string filename = (pos != std::string::npos) 
			? filePath.substr(pos + 1) 
			: filePath;
		response->setHeader("Content-Disposition", 
			"attachment; filename=\"" + filename + "\"");
	}
	
	return response;
}


HttpResponse* HttpController::serveDirectoryListing(const std::string& dirPath, const std::string& uri) {
	DEBUG_LOG("[HttpController] Generating directory listing for: " << dirPath);
	
	std::string html = FileManager::generateDirectoryListing(dirPath, uri);
	
	DEEP_LOG("[HttpController] Directory listing HTML size: " << html.length() << " bytes");
	
	HttpResponse* response = new HttpResponse();
	response->setStatus(StatusCode::OK);
	response->setBody(html);
	response->setContentType("text/html; charset=utf-8");
	return response;
}


// =========================================================================
// CGI Functions (TODO: Move to CgiService during refactoring)
// =========================================================================

std::string	HttpController::getCgiPath(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf) {
	DEBUG_LOG("[HttpController] Checking for CGI execution");

	// 1. Check if cgi_pass is configured in this location
	if (locConf->opCgiPassDirective.empty()) {
		DEEP_LOG("[HttpController] No cgi_pass directive configured");
		return ""; // No CGI config -> regular request
	}

	DEEP_LOG("[HttpController] cgi_pass directive found");

	// 2. Remove query string from URI for file path resolution
	std::string uri = request->getUri();
	size_t queryPos = uri.find('?');
	if (queryPos != std::string::npos) {
		uri = uri.substr(0, queryPos);
	}

	// 3. Convert request path to actual file system path
	std::string resourcePath = PathResolver::resolvePath(serverConf, locConf, uri);
	if (resourcePath.empty()) {
		ERROR_LOG("[HttpController] Failed to resolve CGI path");
		return ""; // Path resolution failed
	}

	// 4. Check if the file actually exists and is a file (not directory)
	if (FileUtils::pathExists(resourcePath) && !FileUtils::isDirectory(resourcePath)) {
		// CGI script file found! Return the path
		DEBUG_LOG("[HttpController] CGI script found: " << resourcePath);
		return resourcePath;
	}

	// If all conditions are not met, this is not a CGI request
	return "";
}

HttpResponse* HttpController::executeCgi(const HttpRequest* request, const std::string& cgiPath, const ServerContext* serverConf, const LocationContext* locConf) {
	DEBUG_LOG("[HttpController] ===== Executing CGI =====");
	DEBUG_LOG("[HttpController] CGI path: " << cgiPath);

	// Step 1. Execute CGI
	CgiExecutor executor(request, cgiPath, serverConf, locConf);
	std::string cgiOutput = executor.execute(); // fork, exec, pipe all happen here

	// Did execution itself fail? (script not found, no permission, etc.)
	if (cgiOutput.empty()) {
		ERROR_LOG("CGI execution failed for path: " + cgiPath);
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf));
	}

	// Step 2. Parse CGI response
	CgiResponseParser parser;
	HttpResponse* response = parser.parse(cgiOutput);

	// Did parsing fail? (CGI output is malformed)
	if (!response) {
		ERROR_LOG("Failed to parse CGI output from: " + cgiPath);
		// CGI script sent invalid response, so '502 Bad Gateway' is more accurate
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::BAD_GATEWAY, serverConf, locConf));
	}

	// Step 3. Return the HttpResponse
	DEBUG_LOG("[HttpController] CGI execution completed successfully");
	return response;
}

