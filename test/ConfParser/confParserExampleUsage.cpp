#include "ConfParser.hpp"
#include <iostream>

int main() {
    try {
        ConfParser parser;
        
        // 예제 설정 문자열 (당신의 pseudo.conf와 유사)
        std::string config = 
            "http {\n"
            "    client_max_body_size 100M;\n"
            "    \n"
            "    server {\n"
            "        listen 192.168.1.100:8080 default_server;\n"
            "        server_name example.com;\n"
            "        client_max_body_size 2000M;\n"
            "        \n"
            "        location /admin/ {\n"
            "            limit_except GET HEAD {\n"
            "                deny all;\n"
            "            }\n"
            "        }\n"
            "        \n"
            "        location /old-page {\n"
            "            return 301 http://example.com/new-page;\n"
            "        }\n"
            "        \n"
            "        location /static/ {\n"
            "            root /usr/share/nginx/;\n"
            "            autoindex on;\n"
            "            index index.html;\n"
            "        }\n"
            "        \n"
            "        location /cgi-bin/ {\n"
            "            root /usr/lib/cgi-bin;\n"
            "            cgi_pass unix:/var/run/fcgiwrap.socket;\n"
            "            limit_except GET POST {\n"
            "                deny all;\n"
            "            }\n"
            "            client_max_body_size 100M;\n"
            "            autoindex off;\n"
            "        }\n"
            "        \n"
            "        location /uploads/ {\n"
            "            root /src/uploads;\n"
            "            client_max_body_size 1000M;\n"
            "            autoindex off;\n"
            "            limit_except POST PUT {\n"
            "                deny all;\n"
            "            }\n"
            "        }\n"
            "        \n"
            "        location /error.html {\n"
            "            root /usr/share/nginx/html;\n"
            "        }\n"
            "    }\n"
            "    \n"
            "    error_page /error.html;\n"
            "}";

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
        
        // === 추가된 에러 케이스 테스트들 ===
        std::cout << "\n=== 에러 케이스 테스트 ===" << std::endl;
        
        // 1. 중복 지시어 테스트
        std::cout << "1. 중복 지시어 테스트..." << std::endl;
        try {
            std::string duplicate_config = 
                "http {\n"
                "    client_max_body_size 100M;\n"
                "    client_max_body_size 200M;\n"
                "}";
            parser.parseString(duplicate_config);
            std::cout << "  실패: 중복 지시어가 감지되지 않음!" << std::endl;
        } catch (const ConfParserException& e) {
            std::cout << "  성공: " << e.what() << std::endl;
        }
        
        // 2. 빈 값 테스트
        std::cout << "2. 빈 값 테스트..." << std::endl;
        try {
            std::string empty_root_config = 
                "http {\n"
                "    server {\n"
                "        root ;\n"
                "    }\n"
                "}";
            parser.parseString(empty_root_config);
            std::cout << "  실패: 빈 root 값이 감지되지 않음!" << std::endl;
        } catch (const ConfParserException& e) {
            std::cout << "  성공: " << e.what() << std::endl;
        }
        
        // 3. 잘못된 컨텍스트 테스트
        std::cout << "3. 잘못된 컨텍스트 테스트..." << std::endl;
        try {
            std::string wrong_context_config = 
                "http {\n"
                "    server_name wrong.com;\n"
                "}";
            parser.parseString(wrong_context_config);
            std::cout << "  실패: 잘못된 컨텍스트가 감지되지 않음!" << std::endl;
        } catch (const ConfParserException& e) {
            std::cout << "  성공: " << e.what() << std::endl;
        }
        
        // 4. http 블록 없음 테스트
        std::cout << "4. http 블록 없음 테스트..." << std::endl;
        try {
            std::string no_http_config = 
                "server {\n"
                "    listen 80;\n"
                "}";
            parser.parseString(no_http_config);
            std::cout << "  실패: http 블록 없음이 감지되지 않음!" << std::endl;
        } catch (const ConfParserException& e) {
            std::cout << "  성공: " << e.what() << std::endl;
        }
        
        // 5. 잘못된 body size 테스트
        std::cout << "5. 잘못된 body size 테스트..." << std::endl;
        try {
            std::string invalid_size_config = 
                "http {\n"
                "    client_max_body_size 100X;\n"
                "}";
            parser.parseString(invalid_size_config);
            std::cout << "  실패: 잘못된 body size가 감지되지 않음!" << std::endl;
        } catch (const ConfParserException& e) {
            std::cout << "  성공: " << e.what() << std::endl;
        }
        
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