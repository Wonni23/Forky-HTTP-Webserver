#include "ConfParser.hpp"
#include "ConfCascader.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        ConfParser parser;
        ConfCascader cascader;
        
        // 명령행에서 설정 파일 경로 받기 (기본값: test.conf)
        std::string config_file = "test.conf";
        if (argc > 1) {
            config_file = argv[1];
        }
        
        std::cout << "=== ConfCascader 테스트: " << config_file << " ===" << std::endl;
        
        // 1. 실제 파일에서 설정 파싱
        std::cout << "\n1. 설정 파일 파싱 중..." << std::endl;
        ConfigDTO originalConfig = parser.parseFile(config_file);
        
        // 2. 원본 설정 출력
        std::cout << "\n2. 원본 설정 (상속 적용 전):" << std::endl;
        parser.printConfig(originalConfig);
        
        // 3. Cascade 적용
        std::cout << "\n3. 설정 상속 적용 중..." << std::endl;
        ConfigDTO cascadedConfig = cascader.applyCascade(originalConfig);
        
        // 4. Cascade 적용 후 설정 출력
        std::cout << "\n4. 상속 적용 후 설정:" << std::endl;
        parser.printConfig(cascadedConfig);
        
        // 5. 상속 변화 분석
        std::cout << "\n5. 상속 변화 분석:" << std::endl;
        cascader.printCascadeInfo(originalConfig, cascadedConfig);
        
        // 6. 실제 설정에 대한 검증
        std::cout << "\n6. 상속 적용 결과 검증:" << std::endl;
        
        const HttpContext& http = cascadedConfig.httpContext;
        bool hasInheritance = false;
        
        for (size_t i = 0; i < http.serverContexts.size(); i++) {
            const ServerContext& server = http.serverContexts[i];
            
            // 서버 레벨에서 HTTP로부터 상속받은 것들 확인
            std::cout << "\n--- 서버 " << (i + 1) << " 상속 확인 ---" << std::endl;
            
            if (!server.opBodySizeDirective.empty()) {
                std::cout << "서버 Body Size: " << server.opBodySizeDirective[0].size << std::endl;
            }
            if (!server.opRootDirective.empty()) {
                std::cout << "서버 Root: " << server.opRootDirective[0].path << std::endl;
            }
            if (!server.opIndexDirective.empty()) {
                std::cout << "서버 Index: " << server.opIndexDirective[0].filename << std::endl;
            }
            
            // Location 레벨에서 상속받은 것들 확인
            for (size_t j = 0; j < server.locationContexts.size(); j++) {
                const LocationContext& location = server.locationContexts[j];
                
                std::cout << "\nLocation '" << location.path << "' 상속 상태:" << std::endl;
                
                if (!location.opBodySizeDirective.empty()) {
                    std::cout << "  Body Size: " << location.opBodySizeDirective[0].size;
                    // 원본과 비교해서 상속받았는지 확인
                    bool inherited = true;
                    if (i < originalConfig.httpContext.serverContexts.size() && 
                        j < originalConfig.httpContext.serverContexts[i].locationContexts.size()) {
                        const LocationContext& originalLoc = originalConfig.httpContext.serverContexts[i].locationContexts[j];
                        if (!originalLoc.opBodySizeDirective.empty()) {
                            inherited = false;
                        }
                    }
                    std::cout << (inherited ? " (상속됨)" : " (자체 설정)") << std::endl;
                    if (inherited) hasInheritance = true;
                }
                
                if (!location.opRootDirective.empty()) {
                    std::cout << "  Root: " << location.opRootDirective[0].path;
                    // 상속 여부 확인
                    bool inherited = true;
                    if (i < originalConfig.httpContext.serverContexts.size() && 
                        j < originalConfig.httpContext.serverContexts[i].locationContexts.size()) {
                        const LocationContext& originalLoc = originalConfig.httpContext.serverContexts[i].locationContexts[j];
                        if (!originalLoc.opRootDirective.empty()) {
                            inherited = false;
                        }
                    }
                    std::cout << (inherited ? " (상속됨)" : " (자체 설정)") << std::endl;
                    if (inherited) hasInheritance = true;
                }
                
                if (!location.opIndexDirective.empty()) {
                    std::cout << "  Index: " << location.opIndexDirective[0].filename << " (상속 또는 자체 설정)" << std::endl;
                }
                
                if (!location.opAutoindexDirective.empty()) {
                    std::cout << "  Autoindex: " << (location.opAutoindexDirective[0].enabled ? "on" : "off") << std::endl;
                }
            }
        }
        
        if (hasInheritance) {
            std::cout << "\n✓ 설정 상속이 정상적으로 적용되었습니다." << std::endl;
        } else {
            std::cout << "\n! 설정 상속이 적용되지 않았거나 모든 설정이 명시적으로 정의되어 있습니다." << std::endl;
        }
        
        std::cout << "\n=== ConfCascader 테스트 완료 ===" << std::endl;
        
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