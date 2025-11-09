#include "http/handler/PostHandler.hpp"
#include "http/StatusCode.hpp"
#include "utils/PathResolver.hpp"
#include "utils/FileManager.hpp"
#include "utils/Common.hpp"
#include <sys/stat.h> // mkdir, stat
#include <string.h>   // strerror
#include <errno.h>    // errno


HttpResponse* PostHandler::handle(const HttpRequest* request,
                                  const ServerContext* serverConf,
                                  const LocationContext* locConf) {
    if (!request || !serverConf || !locConf) {
        ERROR_LOG("[PostHandler] NULL parameter in POST handler");
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf)
        );
    }

    DEBUG_LOG("[PostHandler] ===== Handling POST request =====");
    DEBUG_LOG("[PostHandler] Body size: " << request->getBody().length() << " bytes");

    std::string uploadRoot = PathResolver::resolvePath(serverConf, locConf, request->getUri());

    if (uploadRoot.empty()) {
        ERROR_LOG("[PostHandler] Failed to resolve upload path");
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::FORBIDDEN, serverConf, locConf)
        );
    }

    if (request->getBody().empty()) {
        DEBUG_LOG("[PostHandler] Empty body - no file created");
        HttpResponse* response = new HttpResponse();
        response->setStatus(StatusCode::OK);
        response->setBody("<html><body><h1>200 OK</h1><p>Empty POST request</p></body></html>");
        response->setContentType("text/html; charset=utf-8");
        return response;
    }

    std::string filePath = FileManager::generateUploadFilePath(uploadRoot);
    DEBUG_LOG("[PostHandler] Generated upload path: " << filePath);

    std::string dir = filePath.substr(0, filePath.find_last_of("/"));
    struct stat st;
    if (::stat(dir.c_str(), &st) != 0) {
        if (::mkdir(dir.c_str(), 0755) == -1) {
            ERROR_LOG("[PostHandler] Failed to create directory: " << dir);
            return new HttpResponse(
                HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf)
            );
        }
        DEBUG_LOG("[PostHandler] Directory created: " << dir);
    }

    if (!FileManager::saveFile(filePath, request->getBody())) {
        ERROR_LOG("[PostHandler] Failed to save file: " << filePath);
        return new HttpResponse(
            HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf)
        );
    }

    DEBUG_LOG("[PostHandler] File uploaded: " << filePath);

    HttpResponse* response = new HttpResponse();
    response->setStatus(StatusCode::CREATED);
    response->setHeader("Location", filePath);
    response->setBody("<html><body><h1>201 Created</h1><p>File uploaded to " + filePath + "</p></body></html>");
    response->setContentType("text/html; charset=utf-8");
    return response;
}