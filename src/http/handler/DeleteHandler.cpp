#include "http/handler/DeleteHandler.hpp"
#include "http/StatusCode.hpp"
#include "utils/PathResolver.hpp"
#include "utils/FileUtils.hpp"
#include "utils/FileManager.hpp"
#include "utils/Common.hpp"


HttpResponse* DeleteHandler::handle(const HttpRequest* request,
                                    const ServerContext* serverConf,
                                    const LocationContext* locConf) {
    if (!request || !serverConf || !locConf) {
        ERROR_LOG("[DeleteHandler] NULL parameter in DELETE handler");
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf)
        );
    }

    DEBUG_LOG("[DeleteHandler] ===== Handling DELETE request =====");

    std::string resourcePath = PathResolver::resolvePath(serverConf, locConf, request->getUri());
    DEBUG_LOG("[DeleteHandler] Resolved path: " << resourcePath);

    if (!FileUtils::pathExists(resourcePath)) {
        ERROR_LOG("[DeleteHandler] Path not found: " << resourcePath);
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::NOT_FOUND, serverConf, locConf)
        );
    }

    if (FileUtils::isDirectory(resourcePath)) {
        ERROR_LOG("[DeleteHandler] Cannot delete directory: " << resourcePath);
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::FORBIDDEN, serverConf, locConf)
        );
    }

    DEEP_LOG("[DeleteHandler] Attempting to delete file: " << resourcePath);

    if (!FileManager::deleteFile(resourcePath)) {
        ERROR_LOG("[DeleteHandler] Failed to delete file: " << resourcePath);
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf)
        );
    }

    DEBUG_LOG("[DeleteHandler] File deleted: " << resourcePath);

    HttpResponse* response = new HttpResponse();
    response->setStatus(StatusCode::OK);
    response->setBody("<html><body><h1>200 OK</h1><p>File deleted successfully</p></body></html>");
    response->setContentType("text/html; charset=utf-8");
    return response;
}