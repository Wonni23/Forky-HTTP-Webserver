#include "http/handler/GetHandler.hpp"
#include "http/StatusCode.hpp"
#include "utils/PathResolver.hpp"
#include "utils/FileUtils.hpp"
#include "utils/FileManager.hpp"
#include "utils/Common.hpp"


HttpResponse* GetHandler::handle(const HttpRequest* request,
                                 const ServerContext* serverConf,
                                 const LocationContext* locConf) {
    if (!request) {
        ERROR_LOG("[GetHandler] Request is NULL");
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, NULL, NULL)
        );
    }
    if (!serverConf) {
        ERROR_LOG("[GetHandler] ServerContext is NULL");
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, NULL, NULL)
        );
    }
    if (!locConf) {
        ERROR_LOG("[GetHandler] LocationContext is NULL");
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, NULL)
        );
    }

    DEBUG_LOG("[GetHandler] ===== Handling GET request =====");

    std::string uri = request->getUri();
    std::string resourcePath = PathResolver::resolvePath(serverConf, locConf, uri);
    DEBUG_LOG("[GetHandler] Resolved path: " << resourcePath);

    if (!FileUtils::pathExists(resourcePath)) {
        ERROR_LOG("[GetHandler] Path not found: " << resourcePath);
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf)
        );
    }

    // 디렉토리 trailing slash 리다이렉트
    if (FileUtils::isDirectory(resourcePath)) {
        if (!uri.empty() && uri[uri.length() - 1] != '/') {
            DEBUG_LOG("[GetHandler] Directory without trailing slash, redirecting: " << uri << " -> " << uri << "/");
            HttpResponse* response = new HttpResponse();
            response->setStatus(301);  // Moved Permanently
            response->setHeader("Location", uri + "/");
            response->setBody("<html><body>Redirecting...</body></html>");
            response->setContentType("text/html; charset=utf-8");
            return response;
        }

        DEBUG_LOG("[GetHandler] Path is directory: " << resourcePath);

        std::string indexPath = PathResolver::findIndexFile(resourcePath, locConf);
        if (!indexPath.empty()) {
            DEBUG_LOG("[GetHandler] Index file found: " << indexPath);
            return serveStaticFile(indexPath, locConf);
        }

        DEEP_LOG("[GetHandler] No index file found");

        if (!locConf->opAutoindexDirective.empty() &&
            locConf->opAutoindexDirective[0].enabled) {
            DEBUG_LOG("[GetHandler] Serving directory listing (autoindex enabled)");
            return serveDirectoryListing(resourcePath, uri);
        }

        ERROR_LOG("[GetHandler] Directory listing forbidden for: " << resourcePath);
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf)
        );
    }

    DEBUG_LOG("[GetHandler] Serving static file: " << resourcePath);
    return serveStaticFile(resourcePath, locConf);
}


HttpResponse* GetHandler::serveStaticFile(const std::string& filePath,
                                          const LocationContext* locConf) {
    DEBUG_LOG("[GetHandler] Reading static file: " << filePath);

    std::string content;

    if (!FileManager::readFile(filePath, content)) {
        ERROR_LOG("[GetHandler] Failed to read file: " << filePath);
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, NULL, locConf)
        );
    }

    std::string mimeType = FileUtils::getMimeTypeFromPath(filePath);
    DEEP_LOG("[GetHandler] File size: " << content.length()
             << " bytes, MIME type: " << mimeType);

    HttpResponse* response = new HttpResponse();
    response->setStatus(StatusCode::OK);
    response->setBody(content);
    response->setContentType(mimeType);

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


HttpResponse* GetHandler::serveDirectoryListing(const std::string& dirPath,
                                                const std::string& uri) {
    DEBUG_LOG("[GetHandler] Generating directory listing for: " << dirPath);

    std::string html = FileManager::generateDirectoryListing(dirPath, uri);

    HttpResponse* response = new HttpResponse();
    response->setStatus(StatusCode::OK);
    response->setBody(html);
    response->setContentType("text/html; charset=utf-8");
    return response;
}