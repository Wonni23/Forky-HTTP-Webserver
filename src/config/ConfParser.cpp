#include "ConfParser.hpp"
#include <cctype>
#include <stdexcept>
#include <set>

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
	bool found_http = false;
	
	while (token_index < tokens.size()) {
		if (getCurrentToken() == "http") {
			if (found_http) {
				throwError("Duplicate 'http' block found in configuration file");
			}

			config.httpContext = parseHttpContext();
			found_http = true;
		} else {
			getNextToken();
		}
	}

	if (!found_http) {
		throwError("No 'http' block found in configuration file");
	}

	return config;
}

void ConfParser::tokenize() {
	tokens.clear();
	current_pos = 0;
	current_line = 1;
	
	while (current_pos < config_content.length()) {
		skipWhitespace();
		skipComments();
		
		if (current_pos >= config_content.length()) {
			break;
		}
		
		size_t token_line = current_line;  // 현재 줄 번호 저장
		std::string token_value = readWord();
		if (!token_value.empty()) {
			tokens.push_back(Token(token_value, token_line));
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
	size_t line = (token_index < tokens.size()) ? tokens[token_index].line_number : current_line;
	std::stringstream ss;
	ss << "Line " << line << ": " << message;
	throw ConfParserException(ss.str(), line);
}

void ConfParser::validateDirectiveContext(const std::string& directive, const std::string& context) {
	// location 컨텍스트에서만 사용 가능한 지시어들
	if (directive == "cgi_pass" && context != "location") {
		throwError("'" + directive + "' directive is only allowed in location context");
	}
	
	if (directive == "limit_except" && context != "location") {
		throwError("'" + directive + "' directive is only allowed in location context");
	}
	
	// server 컨텍스트에서만 사용 가능한 지시어들
	if (directive == "server_name" && context != "server") {
		throwError("'" + directive + "' directive is only allowed in server context");
	}
	
	if (directive == "listen" && context != "server") {
		throwError("'" + directive + "' directive is only allowed in server context");
	}
	
	// http 컨텍스트에서 사용 불가능한 지시어들
	if (directive == "server_name" && context == "http") {
		throwError("'" + directive + "' directive is not allowed in http context");
	}
	
	if (directive == "listen" && context == "http") {
		throwError("'" + directive + "' directive is not allowed in http context");
	}
	
	if (directive == "cgi_pass" && context != "location") {
		throwError("'" + directive + "' directive is only allowed in location context");
	}
	
	if (directive == "limit_except" && context != "location") {
		throwError("'" + directive + "' directive is only allowed in location context");
	}
	
	// 모든 컨텍스트에서 사용 가능한 지시어들 확인
	if (directive == "client_max_body_size") {
		if (context != "http" && context != "server" && context != "location") {
			throwError("'" + directive + "' directive is only allowed in http, server, or location context");
		}
	}
	
	if (directive == "root") {
		if (context != "http" && context != "server" && context != "location") {
			throwError("'" + directive + "' directive is only allowed in http, server, or location context");
		}
	}
	
	if (directive == "index") {
		if (context != "http" && context != "server" && context != "location") {
			throwError("'" + directive + "' directive is only allowed in http, server, or location context");
		}
	}
	
	if (directive == "error_page") {
		if (context != "http" && context != "server" && context != "location") {
			throwError("'" + directive + "' directive is only allowed in http, server, or location context");
		}
	}
	
	// server와 location에서만 사용 가능한 지시어들
	if (directive == "return") {
		if (context != "server" && context != "location") {
			throwError("'" + directive + "' directive is only allowed in server or location context");
		}
	}
	
	if (directive == "autoindex") {
		if (context != "server" && context != "location") {
			throwError("'" + directive + "' directive is only allowed in server or location context");
		}
	}
	
	// 논리적으로 맞지 않는 조합들 체크
	if (directive == "autoindex" && context == "http") {
		throwError("'" + directive + "' directive should not be used in http context (use in server or location)");
	}
	
	// limit_except 내부에서만 사용 가능한 지시어들 (추후 확장 가능)
	// 이 부분은 parseLimitExceptDirective에서 별도로 처리됨
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
	return tokens[token_index].value;
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
			checkDuplicateDirective(httpCtx.opBodySizeDirective, "client_max_body_size", "http");
			validateDirectiveContext(directive, "http");
			httpCtx.opBodySizeDirective.push_back(parseBodySizeDirective());
		} else if (directive == "root") {
			checkDuplicateDirective(httpCtx.opRootDirective, "root", "http");
			validateDirectiveContext(directive, "http");
			httpCtx.opRootDirective.push_back(parseRootDirective());
		} else if (directive == "index") {
			checkDuplicateDirective(httpCtx.opIndexDirective, "index", "http");
			validateDirectiveContext(directive, "http");
			httpCtx.opIndexDirective.push_back(parseIndexDirective());
		} else if (directive == "error_page") {
			checkDuplicateDirective(httpCtx.opErrorPageDirective, "error_page", "http");
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
			checkDuplicateDirective(serverCtx.opListenDirective, "listen", "server");
			validateDirectiveContext(directive, "server");
			serverCtx.opListenDirective.push_back(parseListenDirective());
		} else if (directive == "server_name") {
			checkDuplicateDirective(serverCtx.opServerNameDirective, "server_name", "server");
			validateDirectiveContext(directive, "server");
			serverCtx.opServerNameDirective.push_back(parseServerNameDirective());
		} else if (directive == "client_max_body_size") {
			checkDuplicateDirective(serverCtx.opBodySizeDirective, "client_max_body_size", "server");
			validateDirectiveContext(directive, "server");
			serverCtx.opBodySizeDirective.push_back(parseBodySizeDirective());
		} else if (directive == "return") {
			checkDuplicateDirective(serverCtx.opReturnDirective, "return", "server");
			validateDirectiveContext(directive, "server");
			serverCtx.opReturnDirective.push_back(parseReturnDirective());
		} else if (directive == "root") {
			checkDuplicateDirective(serverCtx.opRootDirective, "root", "server");
			validateDirectiveContext(directive, "server");
			serverCtx.opRootDirective.push_back(parseRootDirective());
		} else if (directive == "autoindex") {
			checkDuplicateDirective(serverCtx.opAutoindexDirective, "autoindex", "server");
			validateDirectiveContext(directive, "server");
			serverCtx.opAutoindexDirective.push_back(parseAutoindexDirective());
		} else if (directive == "index") {
			checkDuplicateDirective(serverCtx.opIndexDirective, "index", "server");
			validateDirectiveContext(directive, "server");
			serverCtx.opIndexDirective.push_back(parseIndexDirective());
		} else if (directive == "error_page") {
			checkDuplicateDirective(serverCtx.opErrorPageDirective, "error_page", "server");
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
			checkDuplicateDirective(locationCtx.opLimitExceptDirective, "limit_except", "location");
			validateDirectiveContext(directive, "location");
			locationCtx.opLimitExceptDirective.push_back(parseLimitExceptDirective());
		} else if (directive == "return") {
			checkDuplicateDirective(locationCtx.opReturnDirective, "return", "location");
			validateDirectiveContext(directive, "location");
			locationCtx.opReturnDirective.push_back(parseReturnDirective());
		} else if (directive == "root") {
			checkDuplicateDirective(locationCtx.opRootDirective, "root", "location");
			validateDirectiveContext(directive, "location");
			locationCtx.opRootDirective.push_back(parseRootDirective());
		} else if (directive == "autoindex") {
			checkDuplicateDirective(locationCtx.opAutoindexDirective, "autoindex", "location");
			validateDirectiveContext(directive, "location");
			locationCtx.opAutoindexDirective.push_back(parseAutoindexDirective());
		} else if (directive == "index") {
			checkDuplicateDirective(locationCtx.opIndexDirective, "index", "location");
			validateDirectiveContext(directive, "location");
			locationCtx.opIndexDirective.push_back(parseIndexDirective());
		} else if (directive == "cgi_pass") {
			checkDuplicateDirective(locationCtx.opCgiPassDirective, "cgi_pass", "location");
			validateDirectiveContext(directive, "location");
			locationCtx.opCgiPassDirective.push_back(parseCgiPassDirective());
		} else if (directive == "client_max_body_size") {
			checkDuplicateDirective(locationCtx.opBodySizeDirective, "client_max_body_size", "location");
			validateDirectiveContext(directive, "location");
			locationCtx.opBodySizeDirective.push_back(parseBodySizeDirective());
		} else if (directive == "error_page") {
			checkDuplicateDirective(locationCtx.opErrorPageDirective, "error_page", "location");
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

    if (address.empty() || address == ";") {
        throwError("listen directive requires an address or port");
    }

    getNextToken();
    
    bool default_server = false;
    if (isCurrentToken("default_server")) {
        default_server = true;
        getNextToken();
    }
    
    expectToken(";");
    
    // 파싱 단계에서 address를 host/port로 분해
    ListenDirective directive(address, default_server);
    directive.parseAddress();  // 여기서 host/port 파싱

    return directive;
}

ServerNameDirective ConfParser::parseServerNameDirective() {
	expectToken("server_name");
	
	std::string name = getCurrentToken();
	if (name.empty() || name == ";") {
		throwError("server_name directive requires a value");
	}
	
	getNextToken();
	
	// 추가 server_name이 있으면 에러
	if (!isCurrentToken(";")) {
		std::string extra_name = getCurrentToken();
		throwError("server_name directive accepts only one value, got: '" + extra_name + "'");
	}
	
	expectToken(";");
	return ServerNameDirective(name);
}

ReturnDirective ConfParser::parseReturnDirective() {
	expectToken("return");
	
	std::string code_str = getCurrentToken();
	if (code_str.empty() || code_str == ";") {
		throwError("return directive requires status code and URL");
	}
	getNextToken();
	
	std::string url = getCurrentToken();
	if (url.empty() || url == ";") {
		throwError("return directive requires URL after status code");
	}
	getNextToken();
	
	expectToken(";");
	
	// 상태 코드 유효성 검사
	int code;
	std::stringstream ss(code_str);
	if (!(ss >> code) || code < 100 || code > 599) {
		throwError("Invalid HTTP status code: " + code_str);
	}
	
	return ReturnDirective(code, url);
}

RootDirective ConfParser::parseRootDirective() {
	expectToken("root");
	std::string path = getCurrentToken();

	if (path.empty() || path == ";") {
		throwError("root directive requires a path value");
	}

	if (path[0] != '/') {
		throwError("root path must be an absolute path starting with '/'");
	}
	getNextToken();
	expectToken(";");
	return RootDirective(path);
}

AutoindexDirective ConfParser::parseAutoindexDirective() {
	expectToken("autoindex");
	std::string value = getCurrentToken();
	
	if (value.empty() || value == ";") {
		throwError("autoindex directive requires a value (on/off)");
	}
	
	if (!isBooleanValue(value)) {
		throwError("autoindex directive accepts only: on, off, true, false, 1, 0");
	}
	
	getNextToken();
	expectToken(";");
	return AutoindexDirective(parseBoolean(value));
}

IndexDirective ConfParser::parseIndexDirective() {
	expectToken("index");
	std::string filename = getCurrentToken();
	
	if (filename.empty() || filename == ";") {
		throwError("index directive requires a filename");
	}
	
	getNextToken();
	expectToken(";");
	return IndexDirective(filename);
}

CgiPassDirective ConfParser::parseCgiPassDirective() {
	expectToken("cgi_pass");
	std::string socket_path = getCurrentToken();
	
	if (socket_path.empty() || socket_path == ";") {
		throwError("cgi_pass directive requires a socket path");
	}
	
	getNextToken();
	expectToken(";");
	return CgiPassDirective(socket_path);
}

ErrorPageDirective ConfParser::parseErrorPageDirective() {
	expectToken("error_page");
	std::string path = getCurrentToken();
	
	if (path.empty() || path == ";") {
		throwError("error_page directive requires a path");
	}
	
	getNextToken();
	expectToken(";");
	return ErrorPageDirective(path);
}

LimitExceptDirective ConfParser::parseLimitExceptDirective() {
	expectToken("limit_except");
	
	LimitExceptDirective limitExcept;

	// 허용 가능한 메서드 목록
	static const char* methods_array[] = { "GET", "HEAD", "POST", "PUT", "DELETE" };
	static const std::set<std::string> valid_methods(
		methods_array, methods_array + sizeof(methods_array)/sizeof(methods_array[0])
	);

	// 허용된 메서드들 파싱
	while (!isCurrentToken("{") && !getCurrentToken().empty()) {
		std::string method = getCurrentToken();

		// 유효한 메서드가 아니면 '{' 가 누락된 상황으로 판단
		if (!valid_methods.count(method)) {
			throwError("Expected '{' after limit_except methods but got '" + method + "'");
		}

		limitExcept.allowed_methods.insert(method);
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
	// deny all이 반드시 필요하다고 가정하는 경우
	if (!limitExcept.deny_all) {
		throwError("limit_except block must contain 'deny all;'");
	}

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
			
			// 누락된 부분 추가!
			if (!location.opBodySizeDirective.empty()) {
				std::cout << "      client_max_body_size: " << location.opBodySizeDirective[0].size << std::endl;
			}
			
			if (!location.opRootDirective.empty()) {
				std::cout << "      root: " << location.opRootDirective[0].path << std::endl;
			}
			
			// 누락된 부분 추가!
			if (!location.opIndexDirective.empty()) {
				std::cout << "      index: " << location.opIndexDirective[0].filename << std::endl;
			}
			
			if (!location.opAutoindexDirective.empty()) {
				std::cout << "      autoindex: " << (location.opAutoindexDirective[0].enabled ? "on" : "off") << std::endl;
			}
			
			// 누락된 부분 추가!
			if (!location.opErrorPageDirective.empty()) {
				std::cout << "      error_page: " << location.opErrorPageDirective[0].path << std::endl;
			}
		}
	}
}