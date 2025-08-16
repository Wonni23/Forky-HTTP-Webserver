#ifndef CONF_PARSER_HPP
#define CONF_PARSER_HPP

#include "ConfigDTO.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

class ConfParser {
private:
    std::string config_content;
    size_t current_pos;
    
    // 토큰화 관련
    std::vector<std::string> tokens;
    size_t token_index;
    
    // 파싱 유틸리티 함수들
    void tokenize();
    void skipWhitespace();
    void skipComments();
    std::string readWord();
    std::string readLine();
    bool isEndOfFile() const;
    
    // 토큰 기반 파싱 함수들
    std::string getCurrentToken() const;
    std::string getNextToken();
    void expectToken(const std::string& expected);
    bool isCurrentToken(const std::string& token) const;
    
    // 컨텍스트 파싱 함수들
    HttpContext parseHttpContext();
    ServerContext parseServerContext();
    LocationContext parseLocationContext();
    
    // 지시어 파싱 함수들
    BodySizeDirective parseBodySizeDirective();
    ListenDirective parseListenDirective();
    ServerNameDirective parseServerNameDirective();
    ReturnDirective parseReturnDirective();
    RootDirective parseRootDirective();
    AutoindexDirective parseAutoindexDirective();
    IndexDirective parseIndexDirective();
    CgiPassDirective parseCgiPassDirective();
    ErrorPageDirective parseErrorPageDirective();
    LimitExceptDirective parseLimitExceptDirective();
    
    // 유틸리티 함수들
    bool isBooleanValue(const std::string& value) const;
    bool parseBoolean(const std::string& value) const;
    std::string trim(const std::string& str) const;
    
public:
    ConfParser();
    ~ConfParser();
    
    // 파일에서 설정 로드 및 파싱
    ConfigDTO parseFile(const std::string& filename);
    ConfigDTO parseString(const std::string& content);
    
    // 디버깅용 출력 함수
    void printConfig(const ConfigDTO& config) const;
};

#endif