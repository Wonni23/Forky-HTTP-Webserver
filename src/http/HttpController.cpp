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

    // HEAD 메서드 거부
    if (request->getMethod() == "HEAD") {
        DEBUG_LOG("[HttpController] HEAD method not allowed");
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::METHOD_NOT_ALLOWED, NULL, NULL)
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

    // ========= 메서드 허용 여부 확인 =======
    if (!RequestRouter::isMethodAllowedInLocation(request->getMethod(), *locConf)) {
        ERROR_LOG("[HttpController] Method not allowed: " << request->getMethod()
                  << " for location: " << locConf->path);
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::METHOD_NOT_ALLOWED, serverConf, locConf)
        );
    }

    DEEP_LOG("[HttpController] Method allowed: " << request->getMethod());

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

    ERROR_LOG("[HttpController] Unsupported method: " << method);
    return new HttpResponse(
        HttpResponse::createErrorResponse(StatusCode::NOT_IMPLEMENTED, serverConf, locConf)
    );
}

// ========= GET 요청 처리 =======
HttpResponse* HttpController::handleGetRequest(
    const HttpRequest* request,
    const ServerContext* serverConf,
    const LocationContext* locConf) {

    if (!request) {
        ERROR_LOG("[HttpController] Request is NULL");
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, NULL, NULL)
        );
    }

    if (!serverConf) {
        ERROR_LOG("[HttpController] ServerContext is NULL");
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, NULL, NULL)
        );
    }

    if (!locConf) {
        ERROR_LOG("[HttpController] LocationContext is NULL");
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, NULL)
        );
    }

    DEBUG_LOG("[HttpController] ===== Handling GET request =====");

    std::string uri = request->getUri();
    std::string resourcePath = PathResolver::resolvePath(serverConf, locConf, uri);
    DEBUG_LOG("[HttpController] Resolved path: " << resourcePath);

    if (!FileUtils::pathExists(resourcePath)) {
        ERROR_LOG("[HttpController] Path not found: " << resourcePath);
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf)
        );
    }

    if (FileUtils::isDirectory(resourcePath)) {
        DEBUG_LOG("[HttpController] Path is directory: " << resourcePath);

        std::string indexPath = PathResolver::findIndexFile(resourcePath, locConf);
        if (!indexPath.empty()) {
            DEBUG_LOG("[HttpController] Index file found: " << indexPath);
            return serveStaticFile(indexPath, locConf);
        }

        DEEP_LOG("[HttpController] No index file found");

        if (!locConf->opAutoindexDirective.empty() &&
            locConf->opAutoindexDirective[0].enabled) {
            DEBUG_LOG("[HttpController] Serving directory listing (autoindex enabled)");
            return serveDirectoryListing(resourcePath, uri);
        }

        ERROR_LOG("[HttpController] Directory listing forbidden for: " << resourcePath);
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf)
        );
    }

    DEBUG_LOG("[HttpController] Serving static file: " << resourcePath);
    return serveStaticFile(resourcePath, locConf);
}

// ========= POST 요청 처리 =======
HttpResponse* HttpController::handlePostRequest(
    const HttpRequest* request,
    const ServerContext* serverConf,
    const LocationContext* locConf) {

    if (!request || !serverConf || !locConf) {
        ERROR_LOG("[HttpController] NULL parameter in POST handler");
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf)
        );
    }

    DEBUG_LOG("[HttpController] ===== Handling POST request =====");
    DEBUG_LOG("[HttpController] Body size: " << request->getBody().length() << " bytes");

    std::string uploadRoot = PathResolver::resolvePath(serverConf, locConf, request->getUri());

    if (uploadRoot.empty()) {
        ERROR_LOG("[HttpController] Failed to resolve upload path");
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::FORBIDDEN, serverConf, locConf)
        );
    }

    if (request->getBody().empty()) {
        DEBUG_LOG("[HttpController] Empty body - no file created");
        HttpResponse* response = new HttpResponse();
        response->setStatus(StatusCode::OK);
        response->setBody("<html><body><h1>200 OK</h1><p>Empty POST request</p></body></html>");
        response->setContentType("text/html; charset=utf-8");
        return response;
    }

    std::string filePath = FileManager::generateUploadFilePath(uploadRoot);
    DEBUG_LOG("[HttpController] Generated upload path: " << filePath);

    std::string dir = filePath.substr(0, filePath.find_last_of("/"));
    struct stat st;
    if (::stat(dir.c_str(), &st) != 0) {
        if (::mkdir(dir.c_str(), 0755) == -1) {
            ERROR_LOG("[HttpController] Failed to create directory: " << dir
                     << " err=" << std::strerror(errno));
            return new HttpResponse(
                HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf)
            );
        }
        DEBUG_LOG("[HttpController] Directory created: " << dir);
    }

    if (!FileManager::saveFile(filePath, request->getBody())) {
        ERROR_LOG("[HttpController] Failed to save file: " << filePath);
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf)
        );
    }

    DEBUG_LOG("[HttpController] File uploaded: " << filePath);

    HttpResponse* response = new HttpResponse();
    response->setStatus(StatusCode::CREATED);
    response->setHeader("Location", filePath);
    response->setBody("<html><body><h1>201 Created</h1><p>File uploaded to " + filePath + "</p></body></html>");
    response->setContentType("text/html; charset=utf-8");
    return response;
}

// ========= DELETE 요청 처리 =======
HttpResponse* HttpController::handleDeleteRequest(
    const HttpRequest* request,
    const ServerContext* serverConf,
    const LocationContext* locConf) {

    if (!request || !serverConf || !locConf) {
        ERROR_LOG("[HttpController] NULL parameter in DELETE handler");
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf)
        );
    }

    DEBUG_LOG("[HttpController] ===== Handling DELETE request =====");

    std::string resourcePath = PathResolver::resolvePath(serverConf, locConf, request->getUri());
    DEBUG_LOG("[HttpController] Resolved path: " << resourcePath);

    if (!FileUtils::pathExists(resourcePath)) {
        ERROR_LOG("[HttpController] Path not found: " << resourcePath);
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf)
        );
    }

    if (FileUtils::isDirectory(resourcePath)) {
        ERROR_LOG("[HttpController] Cannot delete directory: " << resourcePath);
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::FORBIDDEN, serverConf, locConf)
        );
    }

    DEEP_LOG("[HttpController] Attempting to delete file: " << resourcePath);

    if (!FileManager::deleteFile(resourcePath)) {
        ERROR_LOG("[HttpController] Failed to delete file: " << resourcePath);
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf)
        );
    }

    DEBUG_LOG("[HttpController] File deleted: " << resourcePath);

    HttpResponse* response = new HttpResponse();
    response->setStatus(StatusCode::OK);
    response->setBody("<html><body><h1>200 OK</h1><p>File deleted successfully</p></body></html>");
    response->setContentType("text/html; charset=utf-8");
    return response;
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

// ========= 정적 파일 제공 =======
HttpResponse* HttpController::serveStaticFile(
    const std::string& filePath,
    const LocationContext* locConf) {

    DEBUG_LOG("[HttpController] Reading static file: " << filePath);

    std::string content;

    if (!FileManager::readFile(filePath, content)) {
        ERROR_LOG("[HttpController] Failed to read file: " << filePath);
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, NULL, locConf)
        );
    }

    std::string mimeType = FileUtils::getMimeTypeFromPath(filePath);
    DEEP_LOG("[HttpController] File size: " << content.length()
             << " bytes, MIME type: " << mimeType);

    HttpResponse* response = new HttpResponse();
    response->setStatus(StatusCode::OK);
    response->setBody(content);
    response->setContentType(mimeType);

    // Content-Disposition 설정
    if (mimeType.find("text/") == 0 || mimeType.find("image/") == 0 ||
        mimeType == "application/pdf" || mimeType == "application/json" ||
        mimeType == "application/javascript") {
        response->setHeader("Content-Disposition", "inline");
    } else {
        size_t pos = filePath.find_last_of('/');
        std::string filename = (pos != std::string::npos) ? filePath.substr(pos + 1) : filePath;
        response->setHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    }

    return response;
}

// ========= 디렉토리 목록 제공 =======
HttpResponse* HttpController::serveDirectoryListing(
    const std::string& dirPath,
    const std::string& uri) {

    DEBUG_LOG("[HttpController] Generating directory listing for: " << dirPath);

    std::string html = FileManager::generateDirectoryListing(dirPath, uri);

    DEEP_LOG("[HttpController] Directory listing HTML size: " << html.length() << " bytes");

    HttpResponse* response = new HttpResponse();
    response->setStatus(StatusCode::OK);
    response->setBody(html);
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

    // ✅ 스크립트의 실제 파일시스템 경로 반환 (변경!)
    std::string scriptPath = PathResolver::resolvePath(serverConf, locConf, uri);
    DEBUG_LOG("[HttpController] CGI script path (resolved): " << scriptPath);

    // 스크립트가 존재하는지 확인
    if (!FileUtils::pathExists(scriptPath)) {
        DEBUG_LOG("[HttpController] CGI script not found: " << scriptPath);
        return "";
    }

    DEBUG_LOG("[HttpController] CGI script found: " << scriptPath);
    return scriptPath;  // ← 스크립트 경로 반환!
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
