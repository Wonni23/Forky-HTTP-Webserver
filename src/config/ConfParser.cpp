#include "ConfParser.hpp"
#include <cctype>
#include <stdexcept>

ConfParser::ConfParser() : current_pos(0), token_index(0) {}

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
    tokens.clear();
    token_index = 0;
    
    tokenize();
    
    ConfigDTO config;
    
    // http 블록 찾기
    while (token_index < tokens.size()) {
        if (getCurrentToken() == "http") {
            config.httpContext = parseHttpContext();
            break;
        }
        getNextToken();
    }
    
    return config;
}

void ConfParser::tokenize() {
    tokens.clear();
    current_pos = 0;
    
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

void ConfParser::skipWhitespace() {
    while (current_pos < config_content.length() && 
           std::isspace(config_content[current_pos])) {
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
        throw std::runtime_error("Expected '" + expected + "' but got '" + current + "'");
    }
    getNextToken();
}

bool ConfParser::isCurrentToken(const std::string& token) const {
    return getCurrentToken() == token;
}

HttpContext ConfParser::parseHttpContext() {
    HttpContext httpCtx;
    
    expectToken("http");
    expectToken("{");
    
    while (!isCurrentToken("}") && !getCurrentToken().empty()) {
        std::string directive = getCurrentToken();
        
        if (directive == "server") {
            httpCtx.serverContexts.push_back(parseServerContext());
        } else if (directive == "client_max_body_size") {
            httpCtx.opBodySizeDirective.push_back(parseBodySizeDirective());
        } else if (directive == "root") {
            httpCtx.opRootDirective.push_back(parseRootDirective());
        } else if (directive == "index") {
            httpCtx.opIndexDirective.push_back(parseIndexDirective());
        } else if (directive == "error_page") {
            httpCtx.opErrorPageDirective.push_back(parseErrorPageDirective());
        } else {
            getNextToken(); // 알 수 없는 지시어 건너뛰기
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
            serverCtx.opListenDirective.push_back(parseListenDirective());
        } else if (directive == "server_name") {
            serverCtx.opServerNameDirective.push_back(parseServerNameDirective());
        } else if (directive == "client_max_body_size") {
            serverCtx.opBodySizeDirective.push_back(parseBodySizeDirective());
        } else if (directive == "return") {
            serverCtx.opReturnDirective.push_back(parseReturnDirective());
        } else if (directive == "root") {
            serverCtx.opRootDirective.push_back(parseRootDirective());
        } else if (directive == "autoindex") {
            serverCtx.opAutoindexDirective.push_back(parseAutoindexDirective());
        } else if (directive == "index") {
            serverCtx.opIndexDirective.push_back(parseIndexDirective());
        } else if (directive == "error_page") {
            serverCtx.opErrorPageDirective.push_back(parseErrorPageDirective());
        } else {
            getNextToken(); // 알 수 없는 지시어 건너뛰기
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
            locationCtx.opLimitExceptDirective.push_back(parseLimitExceptDirective());
        } else if (directive == "return") {
            locationCtx.opReturnDirective.push_back(parseReturnDirective());
        } else if (directive == "root") {
            locationCtx.opRootDirective.push_back(parseRootDirective());
        } else if (directive == "autoindex") {
            locationCtx.opAutoindexDirective.push_back(parseAutoindexDirective());
        } else if (directive == "index") {
            locationCtx.opIndexDirective.push_back(parseIndexDirective());
        } else if (directive == "cgi_pass") {
            locationCtx.opCgiPassDirective.push_back(parseCgiPassDirective());
        } else if (directive == "client_max_body_size") {
            locationCtx.opBodySizeDirective.push_back(parseBodySizeDirective());
        } else if (directive == "error_page") {
            locationCtx.opErrorPageDirective.push_back(parseErrorPageDirective());
        } else {
            getNextToken(); // 알 수 없는 지시어 건너뛰기
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
    std::string name = getCurrentToken();
    getNextToken();
    expectToken(";");
    return ServerNameDirective(name);
}

ReturnDirective ConfParser::parseReturnDirective() {
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

RootDirective ConfParser::parseRootDirective() {
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

LimitExceptDirective ConfParser::parseLimitExceptDirective() {
    expectToken("limit_except");
    
    LimitExceptDirective limitExcept;
    
    // 허용된 메서드들 파싱
    while (!isCurrentToken("{") && !getCurrentToken().empty()) {
        limitExcept.allowed_methods.push_back(getCurrentToken());
        getNextToken();
    }
    
    expectToken("{");
    
    while (!isCurrentToken("}") && !getCurrentToken().empty()) {
        if (isCurrentToken("deny")) {
            getNextToken();
            if (isCurrentToken("all")) {
                limitExcept.deny_all = true;
                getNextToken();
                expectToken(";");
            }
        } else {
            getNextToken(); // 다른 지시어 건너뛰기
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