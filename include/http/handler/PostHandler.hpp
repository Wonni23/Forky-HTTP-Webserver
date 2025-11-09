#pragma once

#include "http/HttpResponse.hpp"
#include "http/HttpRequest.hpp"
#include "dto/ConfigDTO.hpp"

/**
 * @class PostHandler
 * @brief POST 요청을 전문적으로 처리하는 static 유틸리티 클래스
 */
class PostHandler {
public:
    // POST 요청 로직의 메인 진입점
    static HttpResponse* handle(const HttpRequest* request,
                                const ServerContext* serverConf,
                                const LocationContext* locConf);
};