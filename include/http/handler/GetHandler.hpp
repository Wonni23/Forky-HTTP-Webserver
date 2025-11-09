#pragma once

#include "http/HttpResponse.hpp"
#include "http/HttpRequest.hpp"
#include "dto/ConfigDTO.hpp"

/**
 * @class GetHandler
 * @brief GET 요청을 전문적으로 처리하는 static 유틸리티 클래스
 */
class GetHandler {
public:
    // GET 요청 로직의 메인 진입점
    static HttpResponse* handle(const HttpRequest* request,
                                const ServerContext* serverConf,
                                const LocationContext* locConf);

private:
    // HttpController에서 옮겨온 private 헬퍼 함수들
    static HttpResponse* serveStaticFile(const std::string& filePath,
                                         const LocationContext* locConf);
    
    static HttpResponse* serveDirectoryListing(const std::string& dirPath,
                                               const std::string& uri);
};