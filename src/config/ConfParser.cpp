#include "ConfParser.hpp"
#include <cctype>
#include <stdexcept>

ConfParser::ConfParser() : current_pos(0), current_line(1), token_index(0) {}

ConfParser::~ConfParser() {}

ConfigDTO ConfParser::parseFile(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + filename);
    }
    
    std::string content;
    std::string line;
    while (std::getline(file, line)) {
        content += line + "\n";
    }
    file.close();
    
    return parseString(content);
}

ConfigDTO ConfParser::parseString(const std::string& content) {
    config_content = content;
    current_pos = 0;
    current_line = 1;
    tokens.clear();
    token_index = 0;
    
    tokenize();
    
    ConfigDTO config;
    
    // http 블록 찾기
    while (token_index < tokens.size()) {   // gihokim: http 블록이 하나도 없으면 어떻게 되는거지?
        if (getCurrentToken() == "http") {
            config.httpContext = parseHttpContext();
            break;
        }
        getNextToken();
    }
    
    return config;
}

void ConfParser::tokenize() {
    tokens.clear(); // gihokim: 여기서 tokens.clear() 를 호출하는 이유는?
    current_pos = 0;
    current_line = 1;
    
    while (current_pos < config_content.length()) {
        skipWhitespace();
        skipComments();
        
        if (current_pos >= config_content.length()) {
            break;
        }
        
        std::string token = readWord();
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
}

std::string ConfParser::readLine() {
    std::string line;
    while (current_pos < config_content.length() && 
           config_content[current_pos] != '\n') {
        line += config_content[current_pos];
        current_pos++;
    }
    if (current_pos < config_content.length() && 
        config_content[current_pos] == '\n') {
        current_line++;
        current_pos++;
    }
    return line;
}

bool ConfParser::isEndOfFile() const {
    return current_pos >= config_content.length();
}

bool ConfParser::isBooleanValue(const std::string& value) const {
    return value == "on" || value == "off" || 
           value == "true" || value == "false" ||
           value == "1" || value == "0";
}

void ConfParser::throwError(const std::string& message) {
    std::stringstream ss;
    ss << "Line " << current_line << ": " << message;
    throw ConfParserException(ss.str(), current_line);
}

void ConfParser::validateDirectiveContext(const std::string& directive, const std::string& context) {
    // location 컨텍스트에서만 사용 가능한 지시어 체크
    if (directive == "cgi_pass" && context != "location") {
        throwError("'" + directive + "' directive is only allowed in location context");
    }
    
    if (directive == "server_name" && context != "server") {
        throwError("'" + directive + "' directive is only allowed in server context");
    }
    
    if (directive == "listen" && context != "server") {
        throwError("'" + directive + "' directive is only allowed in server context");
    }
    
    if (directive == "limit_except" && context != "location") {
        throwError("'" + directive + "' directive is only allowed in location context");
    }
    
    // 더 많은 검증 규칙들...
}

bool ConfParser::isValidBodySize(const std::string& size) {
    if (size.empty()) return false;
    
    char unit = size[size.length() - 1];
    if (unit != 'K' && unit != 'M' && unit != 'G' && !std::isdigit(unit)) { // gihokim: unit 이 digit 이 아닌지 확인하는 이유는?
        return false;
    }
    
    // 숫자 부분 검증
    size_t unit_offset = std::isalpha(unit) ? 1 : 0;
    for (size_t i = 0; i < size.length() - unit_offset; i++) {
        if (!std::isdigit(size[i])) {
            return false;
        }
    }
    
    return true;
}

void ConfParser::skipWhitespace() {
    while (current_pos < config_content.length() && 
           std::isspace(config_content[current_pos])) {
        if (config_content[current_pos] == '\n') {
            current_line++;
        }
        current_pos++;
    }
}

void ConfParser::skipComments() {
    if (current_pos < config_content.length() && config_content[current_pos] == '#') {
        while (current_pos < config_content.length() && 
               config_content[current_pos] != '\n') {
            current_pos++;
        }
        if (current_pos < config_content.length()) {
            current_line++;
            current_pos++; // '\n' 건너뛰기
        }
    }
}

std::string ConfParser::readWord() {
    skipWhitespace();
    skipComments();
    skipWhitespace();
    
    if (current_pos >= config_content.length()) {
        return "";
    }
    
    std::string word;
    
    // 특수 문자들 (구분자)
    if (config_content[current_pos] == '{' || 
        config_content[current_pos] == '}' || 
        config_content[current_pos] == ';') {
        word += config_content[current_pos];
        current_pos++;
        return word;
    }
    
    // 일반 단어 읽기
    while (current_pos < config_content.length() &&
           !std::isspace(config_content[current_pos]) &&
           config_content[current_pos] != '{' &&
           config_content[current_pos] != '}' &&
           config_content[current_pos] != ';' &&
           config_content[current_pos] != '#') {
        word += config_content[current_pos];
        current_pos++;
    }

    return word;
}

std::string ConfParser::getCurrentToken() const {
    if (token_index >= tokens.size()) {
        return "";
    }
    return tokens[token_index];
}

std::string ConfParser::getNextToken() {
    if (token_index < tokens.size()) {
        token_index++;
    }
    return getCurrentToken();
}

void ConfParser::expectToken(const std::string& expected) {
    std::string current = getCurrentToken();
    if (current != expected) {
        throwError("Expected '" + expected + "' but got '" + current + "'");
    }
    getNextToken(); // gihokim: 그냥 ++ 만 하는게 아니라 getNextToken 을 하는 이유는?
}

bool ConfParser::isCurrentToken(const std::string& token) const {
    return getCurrentToken() == token;
}

HttpContext ConfParser::parseHttpContext() { // gihokim: op(optional) 관련 변수에 2개 이상 있는지는 언제 체크하는거지?
    HttpContext httpCtx;

    expectToken("http");
    expectToken("{");

    while (!isCurrentToken("}") && !getCurrentToken().empty()) {
        std::string directive = getCurrentToken();
        
        if (directive == "server") {
            httpCtx.serverContexts.push_back(parseServerContext());
        } else if (directive == "client_max_body_size") {
            validateDirectiveContext(directive, "http");
            httpCtx.opBodySizeDirective.push_back(parseBodySizeDirective());
        } else if (directive == "root") {
            validateDirectiveContext(directive, "http");
            httpCtx.opRootDirective.push_back(parseRootDirective());
        } else if (directive == "index") {
            validateDirectiveContext(directive, "http");
            httpCtx.opIndexDirective.push_back(parseIndexDirective());
        } else if (directive == "error_page") {
            validateDirectiveContext(directive, "http");
            httpCtx.opErrorPageDirective.push_back(parseErrorPageDirective());
        } else {
            throwError("Unknown directive '" + directive + "' in http context");
        }
    }
    
    expectToken("}");
    return httpCtx;
}

ServerContext ConfParser::parseServerContext() {
    ServerContext serverCtx;
    
    expectToken("server");
    expectToken("{");
    
    while (!isCurrentToken("}") && !getCurrentToken().empty()) {
        std::string directive = getCurrentToken();
        
        if (directive == "location") {
            serverCtx.locationContexts.push_back(parseLocationContext());
        } else if (directive == "listen") {
            validateDirectiveContext(directive, "server");
            serverCtx.opListenDirective.push_back(parseListenDirective());
        } else if (directive == "server_name") {
            validateDirectiveContext(directive, "server");
            serverCtx.opServerNameDirective.push_back(parseServerNameDirective());
        } else if (directive == "client_max_body_size") {
            validateDirectiveContext(directive, "server");
            serverCtx.opBodySizeDirective.push_back(parseBodySizeDirective());
        } else if (directive == "return") {
            validateDirectiveContext(directive, "server");
            serverCtx.opReturnDirective.push_back(parseReturnDirective());
        } else if (directive == "root") {
            validateDirectiveContext(directive, "server");
            serverCtx.opRootDirective.push_back(parseRootDirective());
        } else if (directive == "autoindex") {
            validateDirectiveContext(directive, "server");
            serverCtx.opAutoindexDirective.push_back(parseAutoindexDirective());
        } else if (directive == "index") {
            validateDirectiveContext(directive, "server");
            serverCtx.opIndexDirective.push_back(parseIndexDirective());
        } else if (directive == "error_page") {
            validateDirectiveContext(directive, "server");
            serverCtx.opErrorPageDirective.push_back(parseErrorPageDirective());
        } else {
            throwError("Unknown directive '" + directive + "' in server context");
        }
    }
    
    expectToken("}");
    return serverCtx;
}

LocationContext ConfParser::parseLocationContext() {
    expectToken("location");
    std::string path = getCurrentToken();
    getNextToken();
    
    LocationContext locationCtx(path);
    expectToken("{");
    
    while (!isCurrentToken("}") && !getCurrentToken().empty()) {
        std::string directive = getCurrentToken();
        
        if (directive == "limit_except") {
            validateDirectiveContext(directive, "location");
            locationCtx.opLimitExceptDirective.push_back(parseLimitExceptDirective());
        } else if (directive == "return") {
            validateDirectiveContext(directive, "location");
            locationCtx.opReturnDirective.push_back(parseReturnDirective());
        } else if (directive == "root") {
            validateDirectiveContext(directive, "location");
            locationCtx.opRootDirective.push_back(parseRootDirective());
        } else if (directive == "autoindex") {
            validateDirectiveContext(directive, "location");
            locationCtx.opAutoindexDirective.push_back(parseAutoindexDirective());
        } else if (directive == "index") {
            validateDirectiveContext(directive, "location");
            locationCtx.opIndexDirective.push_back(parseIndexDirective());
        } else if (directive == "cgi_pass") {
            validateDirectiveContext(directive, "location");
            locationCtx.opCgiPassDirective.push_back(parseCgiPassDirective());
        } else if (directive == "client_max_body_size") {
            validateDirectiveContext(directive, "location");
            locationCtx.opBodySizeDirective.push_back(parseBodySizeDirective());
        } else if (directive == "error_page") {
            validateDirectiveContext(directive, "location");
            locationCtx.opErrorPageDirective.push_back(parseErrorPageDirective());
        } else {
            throwError("Unknown directive '" + directive + "' in location context");
        }
    }
    
    expectToken("}");
    return locationCtx;
}

BodySizeDirective ConfParser::parseBodySizeDirective() {
    expectToken("client_max_body_size");
    std::string size = getCurrentToken();
    getNextToken();
    expectToken(";");
    
    // body size 유효성 검사
    if (!isValidBodySize(size)) {
        throwError("Invalid body size format: " + size);
    }
    
    return BodySizeDirective(size);
}

ListenDirective ConfParser::parseListenDirective() {
    expectToken("listen");
    std::string address = getCurrentToken();
    getNextToken();
    
    bool default_server = false;
    if (isCurrentToken("default_server")) {
        default_server = true;
        getNextToken();
    }
    
    expectToken(";");
    return ListenDirective(address, default_server);
}

ServerNameDirective ConfParser::parseServerNameDirective() {
    expectToken("server_name");
    std::string name = getCurrentToken(); // gihokim: 여기에 아무것도 없이 ';'가 바로 오면 어떻게 되지?
    getNextToken();
    expectToken(";");
    return ServerNameDirective(name);
}

ReturnDirective ConfParser::parseReturnDirective() { // gihokim: return 뒤에 바로 ';'가 오면 어떻게 되지?
    expectToken("return");
    
    std::string code_str = getCurrentToken();
    getNextToken();
    
    std::string url = getCurrentToken();
    getNextToken();
    
    expectToken(";");
    
    int code;
    std::stringstream ss(code_str);
    ss >> code;
    
    return ReturnDirective(code, url);
}

RootDirective ConfParser::parseRootDirective() { // gihokim: root 뒤에 바로 ';'가 오면 어떻게 되지?
    expectToken("root");
    std::string path = getCurrentToken();
    getNextToken();
    expectToken(";");
    return RootDirective(path);
}

AutoindexDirective ConfParser::parseAutoindexDirective() {
    expectToken("autoindex");
    std::string value = getCurrentToken();
    getNextToken();
    expectToken(";");
    return AutoindexDirective(parseBoolean(value));
}

IndexDirective ConfParser::parseIndexDirective() {
    expectToken("index");
    std::string filename = getCurrentToken();
    getNextToken();
    expectToken(";");
    return IndexDirective(filename);
}

CgiPassDirective ConfParser::parseCgiPassDirective() {
    expectToken("cgi_pass");
    std::string socket_path = getCurrentToken();
    getNextToken();
    expectToken(";");
    return CgiPassDirective(socket_path);
}

ErrorPageDirective ConfParser::parseErrorPageDirective() {
    expectToken("error_page");
    std::string path = getCurrentToken();
    getNextToken();
    expectToken(";");
    return ErrorPageDirective(path);
}

LimitExceptDirective ConfParser::parseLimitExceptDirective() { // gihokim: 여기 자세히 설명해줘
    expectToken("limit_except");
    
    LimitExceptDirective limitExcept;
    
    // 허용된 메서드들 파싱
    while (!isCurrentToken("{") && !getCurrentToken().empty()) {
        limitExcept.allowed_methods.push_back(getCurrentToken());
        getNextToken();
    }
    
    expectToken("{");
    
    while (!isCurrentToken("}") && !getCurrentToken().empty()) {
        std::string directive = getCurrentToken();
        
        if (directive == "deny") {
            getNextToken();
            if (isCurrentToken("all")) {
                limitExcept.deny_all = true;
                getNextToken();
                expectToken(";");
            } else {
                throwError("Expected 'all' after 'deny'");
            }
        } else {
            throwError("Unknown directive '" + directive + "' in limit_except context");
        }
    }
    
    expectToken("}");
    return limitExcept;
}

bool ConfParser::parseBoolean(const std::string& value) const {
    return value == "on" || value == "true" || value == "1";
}

std::string ConfParser::trim(const std::string& str) const {
    size_t start = 0;
    size_t end = str.length();
    
    while (start < end && std::isspace(str[start])) {
        start++;
    }
    
    while (end > start && std::isspace(str[end - 1])) {
        end--;
    }
    
    return str.substr(start, end - start);
}

void ConfParser::printConfig(const ConfigDTO& config) const {
    std::cout << "=== Parsed Configuration ===" << std::endl;
    std::cout << "HTTP Context:" << std::endl;
    
    // HTTP 레벨 지시어들 출력
    if (!config.httpContext.opBodySizeDirective.empty()) {
        std::cout << "  client_max_body_size: " << config.httpContext.opBodySizeDirective[0].size << std::endl;
    }
    
    // 서버 블록들 출력
    for (size_t i = 0; i < config.httpContext.serverContexts.size(); i++) {
        const ServerContext& server = config.httpContext.serverContexts[i];
        std::cout << "  Server " << (i + 1) << ":" << std::endl;
        
        if (!server.opListenDirective.empty()) {
            std::cout << "    listen: " << server.opListenDirective[0].address;
            if (server.opListenDirective[0].default_server) {
                std::cout << " default_server";
            }
            std::cout << std::endl;
        }
        
        if (!server.opServerNameDirective.empty()) {
            std::cout << "    server_name: " << server.opServerNameDirective[0].name << std::endl;
        }
        
        // Location 블록들 출력
        for (size_t j = 0; j < server.locationContexts.size(); j++) {
            const LocationContext& location = server.locationContexts[j];
            std::cout << "    Location '" << location.path << "':" << std::endl;
            
            if (!location.opRootDirective.empty()) {
                std::cout << "      root: " << location.opRootDirective[0].path << std::endl;
            }
            
            if (!location.opAutoindexDirective.empty()) {
                std::cout << "      autoindex: " << (location.opAutoindexDirective[0].enabled ? "on" : "off") << std::endl;
            }
        }
    }
}