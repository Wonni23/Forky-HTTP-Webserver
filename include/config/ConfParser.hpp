#ifndef CONF_PARSER_HPP
#define CONF_PARSER_HPP

#include "ConfigDTO.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <set>

// 파서 전용 예외 클래스
class ConfParserException : public std::runtime_error {
private:
    size_t line_number;
    std::string context;

public:
    ConfParserException(const std::string& message, size_t line = 0, const std::string& ctx = "")
        : std::runtime_error(message), line_number(line), context(ctx) {}

    virtual ~ConfParserException() throw() {}

    size_t getLineNumber() const { return line_number; }
    const std::string& getContext() const { return context; }
};

struct Token {
    std::string value;
    size_t line_number;
    
    Token(const std::string& val, size_t line) : value(val), line_number(line) {}
};

class ConfParser {
private:
    std::string config_content;
    size_t current_pos;
    size_t current_line;
    
    // 토큰화 관련
    std::vector<Token> tokens;  // 변경
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
    void parseListenAddress(ListenDirective& directive);
    
    // 에러 핸들링 함수들
    void throwError(const std::string& message);
    void validateDirectiveContext(const std::string& directive, const std::string& context);
    bool isValidBodySize(const std::string& size);

    // 중복 지시어 체크 헬퍼 함수 (제네릭)
    template<typename T>
    void checkDuplicateDirective(const std::vector<T>& directiveVector, 
                                const std::string& directiveName, 
                                const std::string& context) {
        if (!directiveVector.empty()) {
            throwError("Duplicate '" + directiveName + "' directive in " + context + " context");
        }
    }
    
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