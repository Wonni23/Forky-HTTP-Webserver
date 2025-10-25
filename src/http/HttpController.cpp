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
	DEBUG_LOG("[HttpController] Method: " << request->getMethod() 
			  << " URI: " << request->getUri() 
			  << " Port: " << connectedPort);
	
	// 0. HEAD 메서드는 무조건 405 (요구사항)
	if (request->getMethod() == "HEAD") {
		DEBUG_LOG("[HttpController] HEAD method requested - returning 405");
		return new HttpResponse(HttpResponse::createErrorResponse(
			StatusCode::METHOD_NOT_ALLOWED, NULL, NULL
		));
	}
	
	// 1. 요청에 맞는 서버 설정 찾기
	const ServerContext* serverConf = RequestRouter::findServerForRequest(request, connectedPort);
	if (!serverConf) {
		ERROR_LOG("[HttpController] No matching server config found for port=" << connectedPort);
		return new HttpResponse(HttpResponse::createErrorResponse(
			StatusCode::INTERNAL_SERVER_ERROR, NULL, NULL
		));
	}
	
	std::string serverName = serverConf->opServerNameDirective.empty() 
		? "(unnamed)" 
		: serverConf->opServerNameDirective[0].name;
	
	// 2. 요청에 맞는 location 찾기
	const LocationContext* locConf = RequestRouter::findLocationForRequest(serverConf, request->getUri(), request->getMethod());

	if (!locConf) {
		// URI 자체가 매칭 안 됨 → 404
		DEBUG_LOG("[HttpController] No matching location for URI: " << request->getUri());
		return new HttpResponse(HttpResponse::createErrorResponse(
			StatusCode::NOT_FOUND, serverConf, NULL
		));
	}
	
	// 3. 메서드 허용 여부 확인
	if (!RequestRouter::isMethodAllowedInLocation(request->getMethod(), *locConf)) {
		ERROR_LOG("[HttpController] Method not allowed: " << request->getMethod() 
				  << " for location: " << locConf->path);
		return new HttpResponse(HttpResponse::createErrorResponse(
			StatusCode::METHOD_NOT_ALLOWED, serverConf, locConf
		));
	}
	
	DEEP_LOG("[HttpController] Method allowed: " << request->getMethod());

	// 4. 리다이렉션 규칙 확인 (return 지시어)
	if (!locConf->opReturnDirective.empty()) {
		DEBUG_LOG("[HttpController] Redirect directive found: code=" 
				  << locConf->opReturnDirective[0].code 
				  << " url=" << locConf->opReturnDirective[0].url);
		return handleRedirect(locConf);
	}

	// CGI 실행 전
	const std::string& method = request->getMethod();
    // CRITICAL CHECK: Ensure body is fully received for methods that require it
    // ----------------------------------------------------------------------------------
    if (method == "POST" || method == "PUT") {
        // Check if the request is marked as complete by the parser/client handler
        if (!request->isComplete()) {
            DEBUG_LOG("[HttpController] Request body incomplete. Cannot proceed with processing. Signaling caller to wait.");
            // Returning NULL here signals the EventLoop/Client to wait for more data
            return NULL;
        }

        // Body size check (moved up to happen before CGI execution or POST handler)
        size_t maxBodySize = locConf->opBodySizeDirective.empty() 
            ? 1024 * 1024 
            : StringUtils::toBytes(locConf->opBodySizeDirective[0].size);
        
        // We enforce it here as a safety measure before any operation (CGI/File upload).
        if (request->getBody().length() > maxBodySize) {
             ERROR_LOG("[HttpController] Body too large: " << request->getBody().length() << " bytes (max: " << maxBodySize << ")");
             return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::PAYLOAD_TOO_LARGE, serverConf, locConf));
        }
    }

	// 5. CGI 실행 여부 확인
	std::string cgiPath = getCgiPath(request, serverConf, locConf);
	if (!cgiPath.empty()) {
		DEBUG_LOG("[HttpController] CGI execution path: " << cgiPath);
		return executeCgi(request, cgiPath, serverConf, locConf);
	}

	// 5-1. CGI location인데 스크립트 파일이 없으면 404
	if (!locConf->opCgiPassDirective.empty()) {
		ERROR_LOG("[HttpController] CGI location but script not found or not executable");
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf));
	}

	// 6. HTTP 메서드에 따라 분기
	if (method == "GET") {
		DEBUG_LOG("[HttpController] Dispatching to GET handler");
		return handleGetRequest(request, serverConf, locConf);
	} 
	else if (method == "POST") {
		DEBUG_LOG("[HttpController] Dispatching to POST handler");
		return handlePostRequest(request, serverConf, locConf);
	} 
	else if (method == "DELETE") {
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
	
	std::string uri = request->getUri();
	std::string resourcePath = PathResolver::resolvePath(serverConf, locConf, uri);
	DEBUG_LOG("[HttpController] Resolved path: " << resourcePath);

	if (!FileUtils::pathExists(resourcePath)) {
		ERROR_LOG("[HttpController] Path not found: " << resourcePath);
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf));
	}

	if (FileUtils::isDirectory(resourcePath)) {
		DEBUG_LOG("[HttpController] Path is directory: " << resourcePath);

		// 1. index 파일 시도
		std::string indexPath = PathResolver::findIndexFile(resourcePath, locConf);
		if (!indexPath.empty()) {
			DEBUG_LOG("[HttpController] Index file found: " << indexPath);
			return serveStaticFile(indexPath, locConf);
		}
		
		DEEP_LOG("[HttpController] No index file found");
		
		// 2. autoindex 허용
		if (!locConf->opAutoindexDirective.empty() && locConf->opAutoindexDirective[0].enabled) {
			DEBUG_LOG("[HttpController] Serving directory listing (autoindex enabled)");
			return serveDirectoryListing(resourcePath, uri);
		}
		
		// 3. 디렉토리 리스팅 금지 → 404
		ERROR_LOG("[HttpController] Directory listing forbidden for: " << resourcePath);
		return new HttpResponse(HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf)); // 404
	}

	// 일반 파일
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

// HttpController.cpp

// 이 함수는 CGI 스크립트의 '실행 경로'를 찾는 데 집중해야 한다.
std::string HttpController::getCgiPath(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf) {
	DEBUG_LOG("[HttpController] Checking for CGI execution");

	(void)serverConf;

	// 1. cgi_pass 지시어가 없으면 CGI 아님
	if (locConf->opCgiPassDirective.empty()) {
		return "";
	}
	
	// 2. URI에서 쿼리 스트링 제거
	std::string uri = request->getUri();
	size_t queryPos = uri.find('?');
	if (queryPos != std::string::npos) {
		uri = uri.substr(0, queryPos);
	}
	
	// 3. cgi_pass 경로 가져오기
	std::string cgiDir = locConf->opCgiPassDirective[0].path;
	DEBUG_LOG("[HttpController] cgi_pass directory: " << cgiDir);
	
	// 4. location path를 제거하고 나머지 경로 추출
	//    예: URI=/cgi-bin/hello.py, location=/cgi-bin/ → hello.py
	// std::string locationPath = locConf->path;
	// std::string scriptRelativePath;
	
	// if (uri.find(locationPath) == 0) {
	// 	scriptRelativePath = uri.substr(locationPath.length());
	// } else {
	// 	ERROR_LOG("[HttpController] URI does not match location path");
	// 	return "";
	// }
	
	// 5. 최종 스크립트 경로 = cgi_pass + 스크립트명
	//    예: /home/donjung/Forky-webserv/www/cgi-bin/ + hello.py
	// std::string scriptPath = cgiDir + scriptRelativePath;
	cgiDir = FileUtils::normalizePath(cgiDir);
	
	// 6. 이제야 파일 존재와 실행 권한 체크!
	// if (!FileUtils::pathExists(scriptPath)) {
	//     ERROR_LOG("[HttpController] CGI script not found: " << scriptPath);
	//     return "";
	// }
	
	// if (FileUtils::isDirectory(scriptPath)) {
	//     ERROR_LOG("[HttpController] Path is directory, not a script: " << scriptPath);
	//     return "";
	// }
	
	// if (!FileUtils::isExecutable(scriptPath)) {
	//     ERROR_LOG("[HttpController] CGI script not executable: " << scriptPath);
	//     return "";
	// }
	
	// DEBUG_LOG("[HttpController] CGI script valid: " << scriptPath);
	return cgiDir; // /home/donjung/Forky-webserv/www/cgi-bin/hello.py
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

