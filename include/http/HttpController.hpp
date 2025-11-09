#pragma once

#include "http/HttpResponse.hpp"
#include "http/HttpRequest.hpp"
#include "dto/ConfigDTO.hpp"

class HttpController {
public:
    // 메인 요청 처리
    static HttpResponse* processRequest(const HttpRequest* request,
                                        int connectedPort,
                                        const ServerContext* serverConf,
                                        const LocationContext* locConf);

private:
    // 공통 헬퍼
    static HttpResponse* handleRedirect(const LocationContext* locConf);
    static std::string   getCgiPath(const HttpRequest* request,
                                    const ServerContext* serverConf,
                                    const LocationContext* locConf);
    static HttpResponse* executeCgi(const HttpRequest* request,
                                    const std::string& cgiPath,
                                    const ServerContext* serverConf,
                                    const LocationContext* locConf);
};