#include "ConfParser.hpp"
#include <iostream>

int main() {
    try {
        ConfParser parser;
        
        // 예제 설정 문자열 (당신의 pseudo.conf와 유사)
        std::string config = R"(
http {
    client_max_body_size 100M;
    
    server {
        listen 192.168.1.100:8080 default_server;
        server_name example.com;
        client_max_body_size 2000M;
        
        location /admin/ {
            limit_except GET HEAD {
                deny all;
            }
        }
        
        location /old-page {
            return 301 http://example.com/new-page;
        }
        
        location /static/ {
            root /usr/share/nginx/;
            autoindex on;
            index index.html;
        }
        
        location /cgi-bin/ {
            root /usr/lib/cgi-bin;
            cgi_pass unix:/var/run/fcgiwrap.socket;
            limit_except GET POST {
                deny all;
            }
            client_max_body_size 100M;
            autoindex off;
        }
        
        location /uploads/ {
            root /src/uploads;
            client_max_body_size 1000M;
            autoindex off;
            limit_except POST PUT {
                deny all;
            }
        }
    }
    
    error_page /error.html;
    location = /error.html {
        root /usr/share/nginx/html;
    }
}
)";

        // 문자열에서 파싱
        ConfigDTO parsedConfig = parser.parseString(config);
        
        // 파싱 결과 출력
        parser.printConfig(parsedConfig);
        
        // 또는 파일에서 파싱하는 경우:
        // ConfigDTO parsedConfig = parser.parseFile("nginx.conf");
        
        std::cout << "\n=== 파싱 성공! ===" << std::endl;
        
        // 파싱된 데이터 활용 예시
        if (!parsedConfig.httpContext.serverContexts.empty()) {
            const ServerContext& firstServer = parsedConfig.httpContext.serverContexts[0];
            
            std::cout << "\n첫 번째 서버 정보:" << std::endl;
            
            if (!firstServer.opListenDirective.empty()) {
                std::cout << "Listen: " << firstServer.opListenDirective[0].address << std::endl;
                std::cout << "Default Server: " << (firstServer.opListenDirective[0].default_server ? "Yes" : "No") << std::endl;
            }
            
            if (!firstServer.opServerNameDirective.empty()) {
                std::cout << "Server Name: " << firstServer.opServerNameDirective[0].name << std::endl;
            }
            
            std::cout << "Location 블록 개수: " << firstServer.locationContexts.size() << std::endl;
            
            // Location 블록들 순회
            for (size_t i = 0; i < firstServer.locationContexts.size(); i++) {
                const LocationContext& loc = firstServer.locationContexts[i];
                std::cout << "  Location " << (i + 1) << ": " << loc.path << std::endl;
                
                if (!loc.opLimitExceptDirective.empty()) {
                    const LimitExceptDirective& limitExcept = loc.opLimitExceptDirective[0];
                    std::cout << "    Allowed Methods: ";
                    for (size_t j = 0; j < limitExcept.allowed_methods.size(); j++) {
                        std::cout << limitExcept.allowed_methods[j];
                        if (j < limitExcept.allowed_methods.size() - 1) {
                            std::cout << ", ";
                        }
                    }
                    std::cout << std::endl;
                    std::cout << "    Deny All: " << (limitExcept.deny_all ? "Yes" : "No") << std::endl;
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "파싱 에러: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}