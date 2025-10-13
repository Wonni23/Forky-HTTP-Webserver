#ifndef CONF_CASCADER_HPP
#define CONF_CASCADER_HPP

#include "dto/ConfigDTO.hpp"
#include <iostream>

class ConfCascader {
private:
    // 개별 지시어 상속 처리 함수들
    void cascadeBodySize(const std::vector<BodySizeDirective>& parent,
                        std::vector<BodySizeDirective>& child) const;
    
    void cascadeRoot(const std::vector<RootDirective>& parent,
                    std::vector<RootDirective>& child) const;
    
    void cascadeIndex(const std::vector<IndexDirective>& parent,
                     std::vector<IndexDirective>& child) const;
    
    void cascadeErrorPage(const std::vector<ErrorPageDirective>& parent,
                         std::vector<ErrorPageDirective>& child) const;
    
    void cascadeAutoindex(const std::vector<AutoindexDirective>& parent,
                         std::vector<AutoindexDirective>& child) const;
    
    // 템플릿 기반 상속 헬퍼 함수
    template<typename T>
    void cascadeDirective(const std::vector<T>& parent,
                         std::vector<T>& child,
                         const std::string& directiveName) const {
        if (child.empty() && !parent.empty()) {
            child = parent;  // 부모 설정 복사
            std::cout << "[CASCADER] Inherited " << directiveName 
                     << " from parent context" << std::endl;
        }
    }
    
    // HTTP -> Server 상속
    void cascadeHttpToServer(const HttpContext& http, ServerContext& server) const;
    
    // Server -> Location 상속  
    void cascadeServerToLocation(const ServerContext& server, LocationContext& location) const;
    
    // HTTP -> Location 상속 (Server를 거치지 않고 직접)
    void cascadeHttpToLocation(const HttpContext& http, LocationContext& location) const;

public:
    ConfCascader();
    ~ConfCascader();
    
    // 메인 cascade 함수 - 전체 ConfigDTO에 상속 적용
    ConfigDTO applyCascade(const ConfigDTO& originalConfig) const;
    
    // 개별 컨텍스트 cascade 함수들
    ServerContext cascadeToServer(const HttpContext& http, const ServerContext& server) const;
    LocationContext cascadeToLocation(const HttpContext& http, 
                                     const ServerContext& server, 
                                     const LocationContext& location) const;
    
    // 디버그용 함수들
    void printCascadeInfo(const ConfigDTO& beforeConfig, const ConfigDTO& afterConfig) const;
    void printContextComparison(const std::string& contextName,
                               const std::string& beforeInfo,
                               const std::string& afterInfo) const;
};

#endif