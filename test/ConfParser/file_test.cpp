#include "ConfParser.hpp"
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    try {
        ConfParser parser;
        
        // 명령행 인수로 설정 파일 경로 받기
        std::string config_file = "test.conf";  // 기본값
        if (argc > 1) {
            config_file = argv[1];
        }
        
        std::cout << "=== 설정 파일 파싱 테스트: " << config_file << " ===" << std::endl;
        
        // 파일에서 설정 파싱
        ConfigDTO parsedConfig = parser.parseFile(config_file);
        
        std::cout << "파일 파싱 성공!" << std::endl;
        
        // 파싱 결과 출력
        parser.printConfig(parsedConfig);
        
        // 상세 정보 출력
        std::cout << "\n=== 상세 분석 ===" << std::endl;
        
        const HttpContext& http = parsedConfig.httpContext;
        
        // HTTP 레벨 설정
        if (!http.opBodySizeDirective.empty()) {
            std::cout << "HTTP 기본 Body Size: " << http.opBodySizeDirective[0].size << std::endl;
        }
        
        if (!http.opErrorPageDirective.empty()) {
            std::cout << "HTTP Error Page Directives (" << http.opErrorPageDirective.size() << "):" << std::endl;
            for (size_t i = 0; i < http.opErrorPageDirective.size(); i++) {
                const ErrorPageDirective& ep = http.opErrorPageDirective[i];
                std::cout << "  Directive " << (i + 1) << ":" << std::endl;
                for (std::map<int, std::string>::const_iterator it = ep.errorPageMap.begin();
                     it != ep.errorPageMap.end(); ++it) {
                    std::cout << "    " << it->first << " -> " << it->second << std::endl;
                }
            }
        }
        
        // 서버 개수
        std::cout << "총 서버 개수: " << http.serverContexts.size() << std::endl;
        
        // 각 서버별 상세 정보
        for (size_t i = 0; i < http.serverContexts.size(); i++) {
            const ServerContext& server = http.serverContexts[i];
            
            std::cout << "\n--- 서버 " << (i + 1) << " ---" << std::endl;
            
            // Listen 정보
            if (!server.opListenDirective.empty()) {
                const ListenDirective& listen = server.opListenDirective[0];
                std::cout << "Listen: " << listen.address;
                if (listen.default_server) {
                    std::cout << " (default_server)";
                }
                std::cout << std::endl;
            }
            
            // Server Name
            if (!server.opServerNameDirective.empty()) {
                std::cout << "Server Name: " << server.opServerNameDirective[0].name << std::endl;
            }
            
            // Body Size
            if (!server.opBodySizeDirective.empty()) {
                std::cout << "Server Body Size: " << server.opBodySizeDirective[0].size << std::endl;
            }
            
            // Return 지시어
            if (!server.opReturnDirective.empty()) {
                const ReturnDirective& ret = server.opReturnDirective[0];
                std::cout << "Server Return: " << ret.code << " " << ret.url << std::endl;
            }
            
            // Location 개수 및 상세
            std::cout << "Location 개수: " << server.locationContexts.size() << std::endl;
            
            for (size_t j = 0; j < server.locationContexts.size(); j++) {
                const LocationContext& location = server.locationContexts[j];
                std::cout << "  Location[" << j << "]: " << location.path << std::endl;
                
                // Root
                if (!location.opRootDirective.empty()) {
                    std::cout << "    Root: " << location.opRootDirective[0].path << std::endl;
                }

                // Alias
                if (!location.opAliasDirective.empty()) {
                    std::cout << "    Alias: " << location.opAliasDirective[0].path << std::endl;
                }

                // Index
                if (!location.opIndexDirective.empty()) {
                    std::cout << "    Index: " << location.opIndexDirective[0].filename << std::endl;
                }
                
                // Autoindex
                if (!location.opAutoindexDirective.empty()) {
                    std::cout << "    Autoindex: " << (location.opAutoindexDirective[0].enabled ? "on" : "off") << std::endl;
                }
                
                // CGI
                if (!location.opCgiPassDirective.empty()) {
                    std::cout << "    CGI Pass: " << location.opCgiPassDirective[0].socket_path << std::endl;
                }
                
                // Return
                if (!location.opReturnDirective.empty()) {
                    const ReturnDirective& ret = location.opReturnDirective[0];
                    std::cout << "    Return: " << ret.code << " " << ret.url << std::endl;
                }
                
                // Limit Except
                if (!location.opLimitExceptDirective.empty()) {
                    const LimitExceptDirective& limit = location.opLimitExceptDirective[0];
                    std::cout << "    Allowed Methods: ";

                    std::set<std::string>::const_iterator it = limit.allowed_methods.begin();
                    for (; it != limit.allowed_methods.end(); ) {
                        std::cout << *it;
                        ++it;
                        if (it != limit.allowed_methods.end()) {
                            std::cout << ", ";
                        }
                    }

                    std::cout << std::endl;

                    if (limit.deny_all) {
                        std::cout << "    Other Methods: DENIED" << std::endl;
                    }
                }
            }
        }
        
        std::cout << "\n=== 테스트 완료 ===" << std::endl;
        
    } catch (const ConfParserException& e) {
        std::cerr << "파싱 에러: " << e.what() << std::endl;
        std::cerr << "줄 번호: " << e.getLineNumber() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "일반 에러: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}