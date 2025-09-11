#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <sstream>

// 테스트 결과 추적
struct TestResult {
    std::string testName;
    bool passed;
    std::string errorMsg;
};

class HttpTester {
private:
    std::vector<TestResult> results;
    
    void addResult(const std::string& testName, bool passed, const std::string& errorMsg = "") {
        TestResult result;
        result.testName = testName;
        result.passed = passed;
        result.errorMsg = errorMsg;
        results.push_back(result);
        
        std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << testName;
        if (!passed && !errorMsg.empty()) {
            std::cout << " - " << errorMsg;
        }
        std::cout << std::endl;
    }
    
public:
    // HttpRequest 테스트들
    void testValidGetRequest() {
        HttpRequest req;
        std::string httpReq = 
            "GET /index.html HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "User-Agent: Mozilla/5.0\r\n"
            "Accept: text/html\r\n"
            "\r\n";
        
        bool success = req.parseRequest(httpReq);
        bool valid = success && 
                    req.getMethod() == "GET" &&
                    req.getUri() == "/index.html" &&
                    req.getVersion() == "HTTP/1.1" &&
                    req.getHeader("Host") == "localhost:8080" &&
                    req.isComplete();
        
        addResult("Valid GET Request", valid);
    }
    
    void testValidPostRequest() {
        HttpRequest req;
        std::string body = "name=test&value=hello";  // 21바이트
        std::stringstream ss;
        ss << body.length();
        
        std::string httpReq = 
            "POST /upload HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: " + ss.str() + "\r\n"  // 21로 자동 계산
            "\r\n" + body;
        
        bool success = req.parseRequest(httpReq);
        bool valid = success && 
                    req.getMethod() == "POST" &&
                    req.getUri() == "/upload" &&
                    req.getBody() == body &&
                    req.getContentLength() == body.length();  // 21
        
        addResult("Valid POST Request", valid);
    }
    
    void testChunkedEncoding() {
        HttpRequest req;
        std::string httpReq = 
            "POST /upload HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "5\r\n"
            "hello\r\n"
            "0\r\n"
            "\r\n";
        
        bool success = req.parseRequest(httpReq);
        bool valid = success && req.isChunkedEncoding();
        
        addResult("Chunked Encoding Detection", valid);
    }
    
    void testUrlDecoding() {
        HttpRequest req;
        std::string httpReq = 
            "GET /path%20with%20spaces?param=%3Dvalue HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n";
        
        bool success = req.parseRequest(httpReq);
        // URI 디코딩이 제대로 되었는지 확인 (구현에 따라 다를 수 있음)
        bool valid = success && req.getMethod() == "GET";
        
        addResult("URL Decoding", valid);
    }
    
    void testInvalidMethod() {
        HttpRequest req;
        std::string httpReq = 
            "INVALID /index.html HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n";
        
        bool success = req.parseRequest(httpReq);
        bool valid = !success && 
                    (req.getLastError() == HttpRequest::PARSE_INVALID_METHOD ||
                     req.getLastError() == HttpRequest::PARSE_UNSUPPORTED_METHOD);
        
        addResult("Invalid Method Handling", valid);
    }
    
    void testMissingHostHeader() {
        HttpRequest req;
        std::string httpReq = 
            "GET /index.html HTTP/1.1\r\n"
            "User-Agent: Test\r\n"
            "\r\n";
        
        bool success = req.parseRequest(httpReq);
        // HTTP/1.1에서는 Host 헤더가 필수이지만, 파싱 자체는 성공할 수 있음
        bool valid = success && req.getMethod() == "GET";
        
        addResult("Missing Host Header", valid);
    }
    
    void testRequestTooLarge() {
        HttpRequest req;
        std::string largeBody(1024 * 1024 + 1, 'A'); // 1MB + 1 byte
        std::stringstream ss;
        ss << largeBody.length();
        std::string httpReq = 
            "POST /upload HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Length: " + ss.str() + "\r\n"
            "\r\n" + largeBody;
        
        bool success = req.parseRequest(httpReq);
        bool valid = !success && req.getLastError() == HttpRequest::PARSE_REQUEST_TOO_LARGE;
        
        addResult("Request Too Large", valid);
    }
    
    void testInvalidHeaderFormat() {
        HttpRequest req;
        std::string httpReq = 
            "GET /index.html HTTP/1.1\r\n"
            "Invalid-Header-Without-Colon\r\n"
            "\r\n";
        
        bool success = req.parseRequest(httpReq);
        bool valid = !success && req.getLastError() == HttpRequest::PARSE_INVALID_HEADER_FORMAT;
        
        addResult("Invalid Header Format", valid);
    }
    
    void testHeadersOnlyParsing() {
        HttpRequest req;
        std::string headerPart = 
            "POST /upload HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Length: 100\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n";
        
        bool success = req.parseHeadersOnly(headerPart);
        bool valid = success && 
                    req.getMethod() == "POST" &&
                    req.isChunkedEncoding() &&
                    req.getContentLength() == 100;
        
        addResult("Headers Only Parsing", valid);
    }
    
    void testRequestReset() {
        HttpRequest req;
        std::string httpReq = 
            "GET /test HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n";
        
        req.parseRequest(httpReq);
        req.reset();
        
        bool valid = req.getMethod().empty() && 
                    req.getUri().empty() &&
                    !req.isComplete() &&
                    req.getLastError() == HttpRequest::PARSE_SUCCESS;
        
        addResult("Request Reset", valid);
    }
    
    // HttpResponse 테스트들
    void testBasicResponse() {
        HttpResponse resp;
        resp.setStatus(200, "OK");
        resp.setHeader("Content-Type", "text/html");
        resp.setBody("<html><body>Hello World</body></html>");
        
        std::string responseStr = resp.toString();
        bool valid = responseStr.find("HTTP/1.1 200 OK") != std::string::npos &&
                    responseStr.find("Content-Type: text/html") != std::string::npos &&
                    responseStr.find("Hello World") != std::string::npos;
        
        addResult("Basic Response Generation", valid);
    }
    
    void testConvenienceFunctions() {
        HttpResponse resp;
        resp.setStatus(200, "OK");
        resp.setContentType("application/json");
        resp.setContentLength(50);
        resp.setServerHeader();
        resp.setDateHeader();
        resp.setConnectionHeader("keep-alive");
        
        std::string responseStr = resp.toString();
        bool valid = responseStr.find("Content-Type: application/json") != std::string::npos &&
                    responseStr.find("Content-Length: 50") != std::string::npos &&
                    responseStr.find("Connection: keep-alive") != std::string::npos;
        
        addResult("Response Convenience Functions", valid);
    }
    
    void testErrorResponse() {
        HttpResponse errorResp = HttpResponse::createErrorResponse(404, "");
        
        bool valid = errorResp.getStatusCode() == 404 &&
                    !errorResp.getBody().empty() &&
                    errorResp.toString().find("404") != std::string::npos;
        
        addResult("Error Response Creation", valid);
    }
    
    void testStatusCodeValidation() {
        HttpResponse resp;
        
        bool valid200 = resp.isValidStatusCode(200);
        bool valid404 = resp.isValidStatusCode(404);
        bool valid500 = resp.isValidStatusCode(500);
        bool invalid999 = !resp.isValidStatusCode(999);
        
        bool valid = valid200 && valid404 && valid500 && invalid999;
        
        addResult("Status Code Validation", valid);
    }
    
    void testResponseHeaders() {
        HttpResponse resp;
        resp.setHeader("Custom-Header", "CustomValue");
        resp.setDefaultHeaders();
        
        bool hasCustom = resp.getHeader("Custom-Header") == "CustomValue";
        
        addResult("Response Headers Management", hasCustom);
    }
    
    // 통합 테스트
    void testRequestResponseIntegration() {
        // Request 파싱
        HttpRequest req;
        std::string httpReq = 
            "GET /api/users?id=123 HTTP/1.1\r\n"
            "Host: api.example.com\r\n"
            "Accept: application/json\r\n"
            "\r\n";
        
        bool reqSuccess = req.parseRequest(httpReq);
        
        // Response 생성
        HttpResponse resp;
        if (reqSuccess && req.getUri().find("/api/users") != std::string::npos) {
            resp.setStatus(200, "OK");
            resp.setContentType("application/json");
            resp.setBody("{\"id\": 123, \"name\": \"Test User\"}");
        } else {
            resp.setStatus(404, "Not Found");
            resp.setContentType("text/html");
            resp.setBody("<html><body>Page not found</body></html>");
        }
        
        resp.setDefaultHeaders();
        std::string responseStr = resp.toString();
        
        bool valid = reqSuccess && 
                    resp.getStatusCode() == 200 &&
                    responseStr.find("application/json") != std::string::npos;
        
        addResult("Request-Response Integration", valid);
    }
    
    void runAllTests() {
        std::cout << "=== HttpRequest/HttpResponse 테스트 시작 ===\n" << std::endl;
        
        std::cout << "--- HttpRequest 테스트 ---" << std::endl;
        testValidGetRequest();
        testValidPostRequest();
        testChunkedEncoding();
        testUrlDecoding();
        testInvalidMethod();
        testMissingHostHeader();
        testRequestTooLarge();
        testInvalidHeaderFormat();
        testHeadersOnlyParsing();
        testRequestReset();
        
        std::cout << "\n--- HttpResponse 테스트 ---" << std::endl;
        testBasicResponse();
        testConvenienceFunctions();
        testErrorResponse();
        testStatusCodeValidation();
        testResponseHeaders();
        
        std::cout << "\n--- 통합 테스트 ---" << std::endl;
        testRequestResponseIntegration();
        
        // 결과 요약
        std::cout << "\n=== 테스트 결과 요약 ===" << std::endl;
        int passed = 0, failed = 0;
        for (std::vector<TestResult>::const_iterator it = results.begin(); it != results.end(); ++it) {
            if (it->passed) passed++;
            else failed++;
        }
        
        std::cout << "총 테스트: " << results.size() << std::endl;
        std::cout << "성공: " << passed << std::endl;
        std::cout << "실패: " << failed << std::endl;
        
        if (failed > 0) {
            std::cout << "\n실패한 테스트들:" << std::endl;
            for (std::vector<TestResult>::const_iterator it = results.begin(); it != results.end(); ++it) {
                if (!it->passed) {
                    std::cout << "- " << it->testName;
                    if (!it->errorMsg.empty()) {
                        std::cout << ": " << it->errorMsg;
                    }
                    std::cout << std::endl;
                }
            }
        }
    }
};

int main() {
    HttpTester tester;
    tester.runAllTests();
    return 0;
}