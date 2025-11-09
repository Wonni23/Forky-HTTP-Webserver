#pragma once

#include "http/HttpResponse.hpp"
#include "http/HttpRequest.hpp"
#include "dto/ConfigDTO.hpp"

class DeleteHandler {
public:
    // DELETE 요청 로직의 메인 진입점
    static HttpResponse* handle(const HttpRequest* request,
                                const ServerContext* serverConf,
                                const LocationContext* locConf);
};