#include "cgi/CgiExecuter.hpp"
#include "http/HttpRequest.hpp"
#include "dto/ConfigDTO.hpp"
#include <iostream>
#include <cassert>
#include <cstring>
#include <fstream>

// ANSI Color codes
#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define RESET "\033[0m"

int g_testCount = 0;
int g_passCount = 0;
int g_failCount = 0;

void printTestHeader(const std::string& testName) {
	std::cout << "\n" << BLUE << "=== Test " << ++g_testCount << ": " << testName << " ===" << RESET << std::endl;
}

void assertTest(bool condition, const std::string& message) {
	if (condition) {
		std::cout << GREEN << "[PASS] " << RESET << message << std::endl;
		g_passCount++;
	} else {
		std::cout << RED << "[FAIL] " << RESET << message << std::endl;
		g_failCount++;
	}
}

void printSummary() {
	std::cout << "\n" << YELLOW << "=== Test Summary ===" << RESET << std::endl;
	std::cout << "Total: " << g_testCount << " tests" << std::endl;
	std::cout << GREEN << "Passed: " << g_passCount << RESET << std::endl;
	std::cout << RED << "Failed: " << g_failCount << RESET << std::endl;

	if (g_failCount == 0) {
		std::cout << GREEN << "\n✓ All tests passed!" << RESET << std::endl;
	} else {
		std::cout << RED << "\n✗ Some tests failed!" << RESET << std::endl;
	}
}

// Helper: Create mock HttpRequest
HttpRequest* createMockRequest(const std::string& method, const std::string& uri,
                                const std::string& body = "",
                                const std::string& contentType = "") {
	std::string requestStr = method + " " + uri + " HTTP/1.1\r\n";
	requestStr += "Host: localhost:8080\r\n";

	if (!contentType.empty()) {
		requestStr += "Content-Type: " + contentType + "\r\n";
	}

	if (!body.empty()) {
		std::ostringstream ss;
		ss << body.length();
		requestStr += "Content-Length: " + ss.str() + "\r\n";
	}

	requestStr += "\r\n";
	requestStr += body;

	HttpRequest* request = new HttpRequest();
	request->parseRequest(requestStr);
	return request;
}

// Helper: Create mock ServerContext
ServerContext createMockServer(int port = 8080, const std::string& serverName = "localhost") {
	ServerContext server;

	ListenDirective listen("0.0.0.0:" + std::string(1, '0' + (port / 1000)));  // Simplified
	listen.port = port;
	server.opListenDirective.push_back(listen);

	ServerNameDirective name(serverName);
	server.opServerNameDirective.push_back(name);

	return server;
}

// Helper: Create mock LocationContext
LocationContext createMockLocation(const std::string& path,
                                    const std::string& root = "/tmp") {
	LocationContext location(path);

	RootDirective rootDir(root);
	location.opRootDirective.push_back(rootDir);

	return location;
}

// Test 1: Execute simple shell script
void testSimpleShellScript() {
	printTestHeader("Execute simple shell script");

	// Create a simple test script
	std::ofstream script("./test_scripts/hello.sh");
	script << "#!/bin/bash\n";
	script << "echo 'Content-Type: text/plain'\n";
	script << "echo ''\n";
	script << "echo 'Hello from CGI!'\n";
	script.close();
	system("chmod +x ./test_scripts/hello.sh");

	HttpRequest* request = createMockRequest("GET", "/cgi-bin/hello.sh");
	ServerContext server = createMockServer();
	LocationContext location = createMockLocation("/cgi-bin/");

	CgiExecutor executor(request, "./test_scripts/hello.sh", &server, &location);
	std::string output = executor.execute();

	assertTest(!output.empty(), "CGI output should not be empty");
	assertTest(output.find("Content-Type: text/plain") != std::string::npos,
	           "Output should contain Content-Type header");
	assertTest(output.find("Hello from CGI!") != std::string::npos,
	           "Output should contain expected message");

	delete request;
}

// Test 2: Execute Python script
void testPythonScript() {
	printTestHeader("Execute Python script");

	// Create a simple Python CGI script
	std::ofstream script("./test_scripts/test.py");
	script << "#!/usr/bin/env python3\n";
	script << "import os\n";
	script << "print('Content-Type: text/plain')\n";
	script << "print('')\n";
	script << "print('REQUEST_METHOD: ' + os.environ.get('REQUEST_METHOD', 'UNKNOWN'))\n";
	script << "print('SCRIPT_NAME: ' + os.environ.get('SCRIPT_NAME', 'UNKNOWN'))\n";
	script.close();
	system("chmod +x ./test_scripts/test.py");

	HttpRequest* request = createMockRequest("GET", "/test.py?foo=bar");
	ServerContext server = createMockServer();
	LocationContext location = createMockLocation(".py", "/tmp");

	CgiExecutor executor(request, "./test_scripts/test.py", &server, &location);
	std::string output = executor.execute();

	assertTest(!output.empty(), "CGI output should not be empty");
	assertTest(output.find("REQUEST_METHOD: GET") != std::string::npos,
	           "REQUEST_METHOD should be set correctly");
	assertTest(output.find("SCRIPT_NAME: /test.py") != std::string::npos,
	           "SCRIPT_NAME should be set correctly");

	delete request;
}

// Test 3: POST request with body
void testPostWithBody() {
	printTestHeader("POST request with body");

	// Create a script that reads stdin
	std::ofstream script("./test_scripts/echo_post.py");
	script << "#!/usr/bin/env python3\n";
	script << "import sys\n";
	script << "import os\n";
	script << "print('Content-Type: text/plain')\n";
	script << "print('')\n";
	script << "content_length = int(os.environ.get('CONTENT_LENGTH', 0))\n";
	script << "if content_length > 0:\n";
	script << "    body = sys.stdin.read(content_length)\n";
	script << "    print('Body: ' + body)\n";
	script << "else:\n";
	script << "    print('No body')\n";
	script.close();
	system("chmod +x ./test_scripts/echo_post.py");

	HttpRequest* request = createMockRequest("POST", "/echo",
	                                          "name=test&value=123",
	                                          "application/x-www-form-urlencoded");
	ServerContext server = createMockServer();
	LocationContext location = createMockLocation("/");

	CgiExecutor executor(request, "./test_scripts/echo_post.py", &server, &location);
	std::string output = executor.execute();

	assertTest(!output.empty(), "CGI output should not be empty");
	assertTest(output.find("Body: name=test&value=123") != std::string::npos,
	           "Body should be passed to CGI script");

	delete request;
}

// Test 4: Query string handling
void testQueryString() {
	printTestHeader("Query string handling");

	std::ofstream script("./test_scripts/query.py");
	script << "#!/usr/bin/env python3\n";
	script << "import os\n";
	script << "print('Content-Type: text/plain')\n";
	script << "print('')\n";
	script << "print('QUERY_STRING: ' + os.environ.get('QUERY_STRING', 'EMPTY'))\n";
	script.close();
	system("chmod +x ./test_scripts/query.py");

	HttpRequest* request = createMockRequest("GET", "/query.py?name=john&age=25");
	ServerContext server = createMockServer();
	LocationContext location = createMockLocation("/");

	CgiExecutor executor(request, "./test_scripts/query.py", &server, &location);
	std::string output = executor.execute();

	assertTest(!output.empty(), "CGI output should not be empty");
	assertTest(output.find("QUERY_STRING: name=john&age=25") != std::string::npos,
	           "QUERY_STRING should be set correctly");

	delete request;
}

// Test 5: Environment variables
void testEnvironmentVariables() {
	printTestHeader("Environment variables");

	std::ofstream script("./test_scripts/env.py");
	script << "#!/usr/bin/env python3\n";
	script << "import os\n";
	script << "print('Content-Type: text/plain')\n";
	script << "print('')\n";
	script << "print('GATEWAY_INTERFACE: ' + os.environ.get('GATEWAY_INTERFACE', 'MISSING'))\n";
	script << "print('SERVER_PROTOCOL: ' + os.environ.get('SERVER_PROTOCOL', 'MISSING'))\n";
	script << "print('SERVER_NAME: ' + os.environ.get('SERVER_NAME', 'MISSING'))\n";
	script << "print('SERVER_PORT: ' + os.environ.get('SERVER_PORT', 'MISSING'))\n";
	script << "print('REQUEST_METHOD: ' + os.environ.get('REQUEST_METHOD', 'MISSING'))\n";
	script << "print('SCRIPT_FILENAME: ' + os.environ.get('SCRIPT_FILENAME', 'MISSING'))\n";
	script.close();
	system("chmod +x ./test_scripts/env.py");

	HttpRequest* request = createMockRequest("GET", "/env.py");
	ServerContext server = createMockServer();
	LocationContext location = createMockLocation("/");

	CgiExecutor executor(request, "./test_scripts/env.py", &server, &location);
	std::string output = executor.execute();

	assertTest(output.find("GATEWAY_INTERFACE: CGI/1.1") != std::string::npos,
	           "GATEWAY_INTERFACE should be set");
	assertTest(output.find("SERVER_PROTOCOL: HTTP/1.1") != std::string::npos,
	           "SERVER_PROTOCOL should be set");
	assertTest(output.find("SERVER_NAME: localhost") != std::string::npos,
	           "SERVER_NAME should be set");
	assertTest(output.find("SERVER_PORT: 8080") != std::string::npos,
	           "SERVER_PORT should be set");
	assertTest(output.find("REQUEST_METHOD: GET") != std::string::npos,
	           "REQUEST_METHOD should be set");
	assertTest(output.find("SCRIPT_FILENAME:") != std::string::npos,
	           "SCRIPT_FILENAME should be set");

	delete request;
}

// Test 6: HTTP headers to environment variables
void testHttpHeaders() {
	printTestHeader("HTTP headers to environment variables");

	std::ofstream script("./test_scripts/headers.py");
	script << "#!/usr/bin/env python3\n";
	script << "import os\n";
	script << "print('Content-Type: text/plain')\n";
	script << "print('')\n";
	script << "print('HTTP_HOST: ' + os.environ.get('HTTP_HOST', 'MISSING'))\n";
	script << "print('HTTP_USER_AGENT: ' + os.environ.get('HTTP_USER_AGENT', 'MISSING'))\n";
	script << "print('HTTP_ACCEPT: ' + os.environ.get('HTTP_ACCEPT', 'MISSING'))\n";
	script.close();
	system("chmod +x ./test_scripts/headers.py");

	std::string requestStr = "GET /headers.py HTTP/1.1\r\n";
	requestStr += "Host: example.com\r\n";
	requestStr += "User-Agent: TestAgent/1.0\r\n";
	requestStr += "Accept: text/html\r\n";
	requestStr += "\r\n";

	HttpRequest* request = new HttpRequest();
	request->parseRequest(requestStr);

	ServerContext server = createMockServer();
	LocationContext location = createMockLocation("/");

	CgiExecutor executor(request, "./test_scripts/headers.py", &server, &location);
	std::string output = executor.execute();

	assertTest(output.find("HTTP_HOST: example.com") != std::string::npos,
	           "HTTP_HOST should be set from Host header");
	assertTest(output.find("HTTP_USER_AGENT: TestAgent/1.0") != std::string::npos,
	           "HTTP_USER_AGENT should be set from User-Agent header");
	assertTest(output.find("HTTP_ACCEPT: text/html") != std::string::npos,
	           "HTTP_ACCEPT should be set from Accept header");

	delete request;
}

// Test 7: PATH_INFO for prefix locations
void testPathInfoPrefix() {
	printTestHeader("PATH_INFO for prefix locations");

	std::ofstream script("./test_scripts/pathinfo.py");
	script << "#!/usr/bin/env python3\n";
	script << "import os\n";
	script << "print('Content-Type: text/plain')\n";
	script << "print('')\n";
	script << "print('PATH_INFO: [' + os.environ.get('PATH_INFO', '') + ']')\n";
	script.close();
	system("chmod +x ./test_scripts/pathinfo.py");

	HttpRequest* request = createMockRequest("GET", "/cgi-bin/pathinfo.py/extra/path");
	ServerContext server = createMockServer();
	LocationContext location = createMockLocation("/cgi-bin/", "/tmp");

	CgiExecutor executor(request, "./test_scripts/pathinfo.py", &server, &location);
	std::string output = executor.execute();

	assertTest(output.find("PATH_INFO: [pathinfo.py/extra/path]") != std::string::npos,
	           "PATH_INFO should contain extra path for prefix location");

	delete request;
}

// Test 8: Script execution failure handling
void testScriptFailure() {
	printTestHeader("Script execution failure handling");

	// Create a script that exits with error
	std::ofstream script("./test_scripts/error.sh");
	script << "#!/bin/bash\n";
	script << "echo 'This goes to stderr' >&2\n";
	script << "exit 1\n";
	script.close();
	system("chmod +x ./test_scripts/error.sh");

	HttpRequest* request = createMockRequest("GET", "/error.sh");
	ServerContext server = createMockServer();
	LocationContext location = createMockLocation("/");

	CgiExecutor executor(request, "./test_scripts/error.sh", &server, &location);
	std::string output = executor.execute();

	assertTest(output.empty(), "Output should be empty for failed script execution");

	delete request;
}

int main() {
	std::cout << YELLOW << "====================================" << RESET << std::endl;
	std::cout << YELLOW << "  CGI Executor Test Suite" << RESET << std::endl;
	std::cout << YELLOW << "====================================" << RESET << std::endl;

	// Create test scripts directory
	system("mkdir -p ./test_scripts");

	testSimpleShellScript();
	testPythonScript();
	testPostWithBody();
	testQueryString();
	testEnvironmentVariables();
	testHttpHeaders();
	testPathInfoPrefix();
	testScriptFailure();

	// Cleanup
	system("rm -rf ./test_scripts");

	printSummary();

	return (g_failCount > 0) ? 1 : 0;
}
