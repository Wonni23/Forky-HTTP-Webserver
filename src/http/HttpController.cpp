#include "http/HttpController.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/StatusCode.hpp"
#include "http/RequestRouter.hpp"
#include "utils/FileUtils.hpp"
#include "utils/StringUtils.hpp"
#include "utils/PathResolver.hpp"
#include "utils/FileManager.hpp"
#include "cgi/CgiExecutor.hpp"
#include "cgi/CgiResponse.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <cstdlib>
#include "http/handler/GetHandler.hpp"
#include "http/handler/PostHandler.hpp"
#include "http/handler/DeleteHandler.hpp"

// ========= 메인 요청 처리 =======
HttpResponse* HttpController::processRequest(
	const HttpRequest* request,
	int connectedPort,
	const ServerContext* serverConf,
	const LocationContext* locConf) {

	DEBUG_LOG("[HttpController] ===== Processing HTTP request =====");
	DEBUG_LOG("[HttpController] Method: " << request->getMethod()
			  << " URI: " << request->getUri()
			  << " Port: " << connectedPort);

	// ========= NULL 체크 =======
	if (!request) {
		ERROR_LOG("[HttpController] Request is NULL");
		return new HttpResponse(
			HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, NULL, NULL)
		);
	}

	// Server 설정 없음
	if (!serverConf) {
		ERROR_LOG("[HttpController] No matching server config found for port=" << connectedPort);
		return new HttpResponse(
			HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, NULL, NULL)
		);
	}

	// Location 설정 없음
	if (!locConf) {
		DEBUG_LOG("[HttpController] No matching location for URI: " << request->getUri());
		return new HttpResponse(
			HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, NULL)
		);
	}

	// ========= Redirect 처리 =======
	if (!locConf->opReturnDirective.empty()) {
		DEBUG_LOG("[HttpController] Redirect directive found: code="
				  << locConf->opReturnDirective[0].code
				  << " url=" << locConf->opReturnDirective[0].url);
		return handleRedirect(locConf);
	}

	// ========= CGI 처리 =======
	std::string cgiPath = getCgiPath(request, serverConf, locConf);
	if (!cgiPath.empty()) {
		DEBUG_LOG("[HttpController] CGI execution path: " << cgiPath);
		return executeCgi(request, cgiPath, serverConf, locConf);
	}

	if (!locConf->opCgiPassDirective.empty()) {
		ERROR_LOG("[HttpController] CGI location but script not found or not executable");
		return new HttpResponse(
			HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf)
		);
	}

	// ========= HTTP 메서드별 처리 =======
	const std::string& method = request->getMethod();

    if (method == "GET") {
		DEBUG_LOG("[HttpController] Dispatching to GET handler");
		return GetHandler::handle(request, serverConf, locConf);
	}
	else if (method == "POST") {
		DEBUG_LOG("[HttpController] Dispatching to POST handler");
		return PostHandler::handle(request, serverConf, locConf);
	}
	else if (method == "DELETE") {
		DEBUG_LOG("[HttpController] Dispatching to DELETE handler");
		return DeleteHandler::handle(request, serverConf, locConf);
	}

	ERROR_LOG("[HttpController] Unsupported method: " << method);
	return new HttpResponse(
		HttpResponse::createErrorResponse(StatusCode::NOT_IMPLEMENTED, serverConf, locConf)
	);

	ERROR_LOG("[HttpController] Unsupported method: " << method);
	return new HttpResponse(
		HttpResponse::createErrorResponse(StatusCode::NOT_IMPLEMENTED, serverConf, locConf)
	);
}	


	// ========= Redirect 처리 =======
HttpResponse* HttpController::handleRedirect(const LocationContext* locConf) {
	if (!locConf || locConf->opReturnDirective.empty()) {
		ERROR_LOG("[HttpController] Invalid redirect configuration");
		return new HttpResponse(
			HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, NULL, locConf)
		);
	}

	const ReturnDirective& redirect = locConf->opReturnDirective[0];

	DEBUG_LOG("[HttpController] Creating redirect: code=" << redirect.code
			  << " url=" << redirect.url);

	HttpResponse* response = new HttpResponse();
	response->setStatus(redirect.code);
	response->setHeader("Location", redirect.url);
	response->setBody("<html><body>Redirecting...</body></html>");
	response->setContentType("text/html; charset=utf-8");
	return response;
}

// ========= CGI 경로 조회 =======
std::string HttpController::getCgiPath(
	const HttpRequest* request,
	const ServerContext* serverConf,
	const LocationContext* locConf) {

	DEBUG_LOG("[HttpController] Checking for CGI execution");

	// cgi_pass가 설정되었는지 확인
	if (!locConf || locConf->opCgiPassDirective.empty()) {
		DEBUG_LOG("[HttpController] No cgi_pass directive found");
		return "";
	}

	// 요청 URI 파싱 (query string 제거)
	std::string uri = request->getUri();
	size_t queryPos = uri.find('?');
	if (queryPos != std::string::npos) {
		uri = uri.substr(0, queryPos);
	}

	// ✅ 스크립트의 실제 파일시스템 경로 반환
	std::string scriptPath = PathResolver::resolvePath(serverConf, locConf, uri);
	DEBUG_LOG("[HttpController] CGI script path (resolved): " << scriptPath);

	// 디렉토리인 경우는 CGI 실행 불가
	if (FileUtils::pathExists(scriptPath) && FileUtils::isDirectory(scriptPath)) {
		DEBUG_LOG("[HttpController] Path is directory, not a CGI script: " << scriptPath);
		return "";
	}

	// CGI location이면 파일 존재 여부와 관계없이 CGI로 전달
	// (ubuntu_cgi_tester는 파일이 없어도 stdin을 처리하고 200을 반환함)
	DEBUG_LOG("[HttpController] CGI script path: " << scriptPath);
	return scriptPath;
}

// ========= CGI 실행 =======
HttpResponse* HttpController::executeCgi(
	const HttpRequest* request,
	const std::string& cgiPath,
	const ServerContext* serverConf,
	const LocationContext* locConf) {

	DEBUG_LOG("[HttpController] ===== Executing CGI =====");
	DEBUG_LOG("[HttpController] CGI path: " << cgiPath);

	CgiExecutor executor(request, cgiPath, serverConf, locConf);
	std::string cgiOutput = executor.execute();

	if (cgiOutput.empty()) {
		ERROR_LOG("[HttpController] CGI execution failed for path: " << cgiPath);
		return new HttpResponse(
			HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf)
		);
	}

	CgiResponseParser parser;
	HttpResponse* response = parser.parse(cgiOutput);

	if (!response) {
		ERROR_LOG("[HttpController] Failed to parse CGI output from: " << cgiPath);
		return new HttpResponse(
			HttpResponse::createErrorResponse(StatusCode::BAD_GATEWAY, serverConf, locConf)
		);
	}

	DEBUG_LOG("[HttpController] CGI execution completed successfully");
	return response;
}
