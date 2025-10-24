#include "cgi/CgiResponse.hpp"
#include "http/HttpResponse.hpp"
#include "http/StatusCode.hpp"
#include <iostream>
#include <cassert>
#include <cstring>

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

// Test 1: Basic CGI output parsing with \r\n
void testBasicCgiOutputWithCRLF() {
	printTestHeader("Basic CGI output parsing with \\r\\n");

	std::string cgiOutput =
		"Content-Type: text/html\r\n"
		"Status: 200 OK\r\n"
		"\r\n"
		"<html><body>Hello World</body></html>";

	CgiResponseParser parser;
	HttpResponse* response = parser.parse(cgiOutput);

	assertTest(response != NULL, "Response should not be NULL");
	if (response) {
		assertTest(response->getStatus() == StatusCode::OK, "Status should be 200");
		assertTest(response->getContentType() == "text/html", "Content-Type should be text/html");
		assertTest(response->getBody() == "<html><body>Hello World</body></html>", "Body should match");
		delete response;
	}
}

// Test 2: Basic CGI output parsing with \n
void testBasicCgiOutputWithLF() {
	printTestHeader("Basic CGI output parsing with \\n");

	std::string cgiOutput =
		"Content-Type: text/plain\n"
		"Status: 200 OK\n"
		"\n"
		"Plain text response";

	CgiResponseParser parser;
	HttpResponse* response = parser.parse(cgiOutput);

	assertTest(response != NULL, "Response should not be NULL");
	if (response) {
		assertTest(response->getStatus() == StatusCode::OK, "Status should be 200");
		assertTest(response->getContentType() == "text/plain", "Content-Type should be text/plain");
		assertTest(response->getBody() == "Plain text response", "Body should match");
		delete response;
	}
}

// Test 3: CGI output with custom status code
void testCustomStatusCode() {
	printTestHeader("CGI output with custom status code");

	std::string cgiOutput =
		"Content-Type: text/html\r\n"
		"Status: 404 Not Found\r\n"
		"\r\n"
		"<html><body>Page Not Found</body></html>";

	CgiResponseParser parser;
	HttpResponse* response = parser.parse(cgiOutput);

	assertTest(response != NULL, "Response should not be NULL");
	if (response) {
		assertTest(response->getStatus() == StatusCode::NOT_FOUND, "Status should be 404");
		assertTest(response->getContentType() == "text/html", "Content-Type should be text/html");
		delete response;
	}
}

// Test 4: CGI output without Status header (should default to 200)
void testDefaultStatus() {
	printTestHeader("CGI output without Status header (default 200)");

	std::string cgiOutput =
		"Content-Type: application/json\r\n"
		"\r\n"
		"{\"message\": \"success\"}";

	CgiResponseParser parser;
	HttpResponse* response = parser.parse(cgiOutput);

	assertTest(response != NULL, "Response should not be NULL");
	if (response) {
		assertTest(response->getStatus() == StatusCode::OK, "Status should default to 200");
		assertTest(response->getContentType() == "application/json", "Content-Type should be application/json");
		delete response;
	}
}

// Test 5: CGI output without Content-Type (should default)
void testDefaultContentType() {
	printTestHeader("CGI output without Content-Type header");

	std::string cgiOutput =
		"Status: 200 OK\r\n"
		"\r\n"
		"<html><body>Test</body></html>";

	CgiResponseParser parser;
	HttpResponse* response = parser.parse(cgiOutput);

	assertTest(response != NULL, "Response should not be NULL");
	if (response) {
		assertTest(response->getContentType() == "text/html; charset=utf-8", "Content-Type should be default");
		delete response;
	}
}

// Test 6: Empty CGI output
void testEmptyCgiOutput() {
	printTestHeader("Empty CGI output");

	std::string cgiOutput = "";

	CgiResponseParser parser;
	HttpResponse* response = parser.parse(cgiOutput);

	assertTest(response == NULL, "Response should be NULL for empty output");
	if (response) {
		delete response;
	}
}

// Test 7: CGI output with no header delimiter
void testNoHeaderDelimiter() {
	printTestHeader("CGI output with no header delimiter");

	std::string cgiOutput = "Content-Type: text/html";

	CgiResponseParser parser;
	HttpResponse* response = parser.parse(cgiOutput);

	assertTest(response == NULL, "Response should be NULL when no header delimiter found");
	if (response) {
		delete response;
	}
}

// Test 8: CGI output with custom headers
void testCustomHeaders() {
	printTestHeader("CGI output with custom headers");

	std::string cgiOutput =
		"Content-Type: text/html\r\n"
		"Status: 200 OK\r\n"
		"X-Custom-Header: CustomValue\r\n"
		"Set-Cookie: session=abc123\r\n"
		"\r\n"
		"<html><body>Test</body></html>";

	CgiResponseParser parser;
	HttpResponse* response = parser.parse(cgiOutput);

	assertTest(response != NULL, "Response should not be NULL");
	if (response) {
		assertTest(response->getStatus() == StatusCode::OK, "Status should be 200");
		assertTest(response->getHeader("X-Custom-Header") == "CustomValue", "Custom header should be set");
		assertTest(response->getHeader("Set-Cookie") == "session=abc123", "Cookie header should be set");
		delete response;
	}
}

// Test 9: CGI output with multiline body
void testMultilineBody() {
	printTestHeader("CGI output with multiline body");

	std::string cgiOutput =
		"Content-Type: text/plain\r\n"
		"\r\n"
		"Line 1\r\n"
		"Line 2\r\n"
		"Line 3";

	CgiResponseParser parser;
	HttpResponse* response = parser.parse(cgiOutput);

	assertTest(response != NULL, "Response should not be NULL");
	if (response) {
		std::string expectedBody = "Line 1\r\nLine 2\r\nLine 3";
		assertTest(response->getBody() == expectedBody, "Multiline body should be preserved");
		delete response;
	}
}

// Test 10: CGI output with case-insensitive headers
void testCaseInsensitiveHeaders() {
	printTestHeader("CGI output with case-insensitive headers");

	std::string cgiOutput1 =
		"content-type: text/html\r\n"
		"status: 200 OK\r\n"
		"\r\n"
		"Body";

	std::string cgiOutput2 =
		"Content-Type: text/html\r\n"
		"Status: 200 OK\r\n"
		"\r\n"
		"Body";

	CgiResponseParser parser;
	HttpResponse* response1 = parser.parse(cgiOutput1);
	HttpResponse* response2 = parser.parse(cgiOutput2);

	assertTest(response1 != NULL, "Response with lowercase headers should not be NULL");
	assertTest(response2 != NULL, "Response with capitalized headers should not be NULL");

	if (response1 && response2) {
		assertTest(response1->getStatus() == response2->getStatus(), "Both should have same status");
		assertTest(response1->getContentType() == response2->getContentType(), "Both should have same content-type");
		delete response1;
		delete response2;
	}
}

// Test 11: CGI output with whitespace in headers
void testWhitespaceInHeaders() {
	printTestHeader("CGI output with extra whitespace in headers");

	std::string cgiOutput =
		"Content-Type:    text/html   \r\n"
		"Status:   201 Created  \r\n"
		"\r\n"
		"Body";

	CgiResponseParser parser;
	HttpResponse* response = parser.parse(cgiOutput);

	assertTest(response != NULL, "Response should not be NULL");
	if (response) {
		assertTest(response->getStatus() == StatusCode::CREATED, "Status should be 201 (trimmed)");
		assertTest(response->getContentType() == "text/html", "Content-Type should be trimmed");
		delete response;
	}
}

// Test 12: CGI output with empty lines in headers
void testEmptyLinesInHeaders() {
	printTestHeader("CGI output with empty lines in headers");

	std::string cgiOutput =
		"Content-Type: text/html\r\n"
		"\r\n"
		"Status: 200 OK\r\n"
		"\r\n"
		"Body";

	CgiResponseParser parser;
	HttpResponse* response = parser.parse(cgiOutput);

	// First \r\n\r\n ends headers, everything after is body
	assertTest(response != NULL, "Response should not be NULL");
	if (response) {
		std::string expectedBody = "Status: 200 OK\r\n\r\nBody";
		assertTest(response->getBody() == expectedBody, "Body should include status line after delimiter");
		delete response;
	}
}

int main() {
	std::cout << YELLOW << "====================================" << RESET << std::endl;
	std::cout << YELLOW << "  CGI Response Parser Test Suite" << RESET << std::endl;
	std::cout << YELLOW << "====================================" << RESET << std::endl;

	testBasicCgiOutputWithCRLF();
	testBasicCgiOutputWithLF();
	testCustomStatusCode();
	testDefaultStatus();
	testDefaultContentType();
	testEmptyCgiOutput();
	testNoHeaderDelimiter();
	testCustomHeaders();
	testMultilineBody();
	testCaseInsensitiveHeaders();
	testWhitespaceInHeaders();
	testEmptyLinesInHeaders();

	printSummary();

	return (g_failCount > 0) ? 1 : 0;
}
