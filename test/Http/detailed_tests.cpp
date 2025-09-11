#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <vector>

// TestCase를 전역 struct로 이동 (local type 문제 해결)
struct TestCase {
    std::string ext;
    std::string expectedType;
};

// 특정 기능들에 대한 상세 테스트

void testHttpRequestEdgeCases() {
    std::cout << "=== HttpRequest 엣지 케이스 테스트 ===" << std::endl;
    
    // 1. 빈 요청
    {
        HttpRequest req;
        bool result = req.parseRequest("");
        std::cout << "빈 요청 처리: " << (!result ? "PASS" : "FAIL") << std::endl;
    }
    
    // 2. 불완전한 헤더
    {
        HttpRequest req;
        std::string incompleteReq = "GET /test HTTP/1.1\r\nHost: localhost";
        bool result = req.parseRequest(incompleteReq);
        std::cout << "불완전한 헤더: " << (!result ? "PASS" : "FAIL") << std::endl;
    }
    
    // 3. 매우 긴 URI
    {
        HttpRequest req;
        std::string longUri(3000, 'a'); // 2KB 초과
        std::string httpReq = "GET /" + longUri + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
        bool result = req.parseRequest(httpReq);
        std::cout << "긴 URI 처리: " << (!result ? "PASS" : "FAIL") << std::endl;
    }
    
    // 4. 대소문자 섞인 헤더
    {
        HttpRequest req;
        std::string httpReq = 
            "GET /test HTTP/1.1\r\n"
            "HOST: localhost\r\n"  // 대문자
            "content-type: text/html\r\n"  // 소문자
            "Content-Length: 0\r\n"  // 혼합
            "\r\n";
        bool result = req.parseRequest(httpReq);
        std::cout << "대소문자 헤더: " << (result ? "PASS" : "FAIL") << std::endl;
        if (result) {
            std::cout << "  Host 헤더: " << req.getHeader("host") << std::endl;
            std::cout << "  Content-Type: " << req.getHeader("content-type") << std::endl;
        }
    }
    
    // 5. 특수 문자가 포함된 헤더 값
    {
        HttpRequest req;
        std::string httpReq = 
            "GET /test HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "User-Agent: Mozilla/5.0 (특수문자: 한글, émojis 🚀)\r\n"
            "\r\n";
        bool result = req.parseRequest(httpReq);
        std::cout << "특수문자 헤더: " << (result ? "PASS" : "FAIL") << std::endl;
    }
    
    // 6. 중복 헤더
    {
        HttpRequest req;
        std::string httpReq = 
            "GET /test HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Accept: text/html\r\n"
            "Accept: application/json\r\n"  // 중복
            "\r\n";
        bool result = req.parseRequest(httpReq);
        std::cout << "중복 헤더: " << (result ? "PASS" : "FAIL") << std::endl;
        if (result) {
            std::cout << "  Accept 값: " << req.getHeader("Accept") << std::endl;
        }
    }
}

void testHttpResponseEdgeCases() {
    std::cout << "\n=== HttpResponse 엣지 케이스 테스트 ===" << std::endl;
    
    // 1. 매우 큰 응답 바디
    {
        HttpResponse resp;
        std::string largeBody(1024 * 1024, 'X'); // 1MB
        resp.setStatus(200, "OK");
        resp.setBody(largeBody);
        resp.setContentLength(largeBody.length());
        
        std::string responseStr = resp.toString();
        bool valid = responseStr.find("Content-Length: 1048576") != std::string::npos;
        std::cout << "큰 응답 바디: " << (valid ? "PASS" : "FAIL") << std::endl;
    }
    
    // 2. 특수 상태 코드들
    {
        std::vector<int> statusCodes;
        statusCodes.push_back(100);
        statusCodes.push_back(101);
        statusCodes.push_back(201);
        statusCodes.push_back(204);
        statusCodes.push_back(301);
        statusCodes.push_back(302);
        statusCodes.push_back(304);
        statusCodes.push_back(400);
        statusCodes.push_back(401);
        statusCodes.push_back(403);
        statusCodes.push_back(404);
        statusCodes.push_back(500);
        statusCodes.push_back(502);
        statusCodes.push_back(503);
        
        bool allValid = true;
        
        for (std::vector<int>::const_iterator it = statusCodes.begin(); it != statusCodes.end(); ++it) {
            HttpResponse resp = HttpResponse::createErrorResponse(*it, "");
            if (resp.getStatusCode() != *it) {
                allValid = false;
                break;
            }
        }
        std::cout << "다양한 상태코드: " << (allValid ? "PASS" : "FAIL") << std::endl;
    }
    
    // 3. 빈 바디 응답
    {
        HttpResponse resp;
        resp.setStatus(204, "No Content");
        resp.setDefaultHeaders();
        
        std::string responseStr = resp.toString();
        bool valid = responseStr.find("204 No Content") != std::string::npos &&
                    resp.getBody().empty();
        std::cout << "빈 바디 응답: " << (valid ? "PASS" : "FAIL") << std::endl;
    }
    
    // 4. 특수 문자 헤더 값
    {
        HttpResponse resp;
        resp.setStatus(200, "OK");
        resp.setHeader("Content-Disposition", "attachment; filename=\"한글파일명.txt\"");
        resp.setHeader("X-Custom", "특수문자: émojis 🎉");
        
        std::string responseStr = resp.toString();
        bool valid = responseStr.find("한글파일명.txt") != std::string::npos;
        std::cout << "특수문자 헤더값: " << (valid ? "PASS" : "FAIL") << std::endl;
    }
}

void testMimeTypeDetection() {
    std::cout << "\n=== MIME 타입 테스트 ===" << std::endl;
    
    // 파일 확장자별 Content-Type 설정 테스트
    std::vector<TestCase> testCases;
    TestCase tc1; tc1.ext = ".html"; tc1.expectedType = "text/html";
    TestCase tc2; tc2.ext = ".css"; tc2.expectedType = "text/css";
    TestCase tc3; tc3.ext = ".js"; tc3.expectedType = "application/javascript";
    TestCase tc4; tc4.ext = ".json"; tc4.expectedType = "application/json";
    TestCase tc5; tc5.ext = ".png"; tc5.expectedType = "image/png";
    TestCase tc6; tc6.ext = ".jpg"; tc6.expectedType = "image/jpeg";
    TestCase tc7; tc7.ext = ".gif"; tc7.expectedType = "image/gif";
    TestCase tc8; tc8.ext = ".pdf"; tc8.expectedType = "application/pdf";
    TestCase tc9; tc9.ext = ".txt"; tc9.expectedType = "text/plain";
    TestCase tc10; tc10.ext = ".unknown"; tc10.expectedType = "application/octet-stream";
    
    testCases.push_back(tc1);
    testCases.push_back(tc2);
    testCases.push_back(tc3);
    testCases.push_back(tc4);
    testCases.push_back(tc5);
    testCases.push_back(tc6);
    testCases.push_back(tc7);
    testCases.push_back(tc8);
    testCases.push_back(tc9);
    testCases.push_back(tc10);
    
    for (std::vector<TestCase>::const_iterator it = testCases.begin(); it != testCases.end(); ++it) {
        HttpResponse resp;
        resp.setStatus(200, "OK");
        
        // 실제 구현에서는 파일 확장자를 기반으로 Content-Type을 설정하는 함수가 있을 것
        // 여기서는 수동으로 설정
        if (it->ext == ".html") resp.setContentType("text/html");
        else if (it->ext == ".css") resp.setContentType("text/css");
        else if (it->ext == ".js") resp.setContentType("application/javascript");
        else resp.setContentType("application/octet-stream");
        
        std::cout << "확장자 " << it->ext << ": ";
        std::cout << "설정됨" << std::endl;
    }
}

void performanceTest() {
    std::cout << "\n=== 성능 테스트 ===" << std::endl;
    
    const int iterations = 1000;
    
    // HttpRequest 파싱 성능
    {
        std::string body = "{\"data\": \"test\", \"number\": 12345, \"array\": [1, 2, 3], \"nested\": {\"key\": \"value\"}}";
        std::stringstream contentLengthSS;
        contentLengthSS << body.length();
        
        std::string httpReq = 
            "POST /api/data HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: " + contentLengthSS.str() + "\r\n"
            "User-Agent: Test Client\r\n"
            "Accept: application/json\r\n"
            "\r\n" + body;
        
        clock_t start = clock();
        int successCount = 0;
        
        for (int i = 0; i < iterations; i++) {
            HttpRequest req;
            if (req.parseRequest(httpReq)) {
                successCount++;
            }
        }
        
        clock_t end = clock();
        double duration = double(end - start) / CLOCKS_PER_SEC;
        
        std::cout << "HttpRequest 파싱 " << iterations << "회: " 
                  << duration * 1000 << "ms" << std::endl;
        std::cout << "성공율: " << (successCount * 100 / iterations) << "%" << std::endl;
    }
    
    // HttpResponse 생성 성능
    {
        clock_t start = clock();
        
        for (int i = 0; i < iterations; i++) {
            HttpResponse resp;
            resp.setStatus(200, "OK");
            resp.setContentType("text/html");
            std::stringstream ss;
            ss << i;
            resp.setBody("<html><body>Test Response " + ss.str() + "</body></html>");
            resp.setDefaultHeaders();
            std::string responseStr = resp.toString();
        }
        
        clock_t end = clock();
        double duration = double(end - start) / CLOCKS_PER_SEC;
        
        std::cout << "HttpResponse 생성 " << iterations << "회: " 
                  << duration * 1000 << "ms" << std::endl;
    }
}

void memoryLeakTest() {
    std::cout << "\n=== 메모리 사용 테스트 ===" << std::endl;
    
    // 반복적인 생성/소멸 테스트
    for (int i = 0; i < 100; i++) {
        HttpRequest* req = new HttpRequest();
        std::stringstream ss1;
        ss1 << i;
        std::string httpReq = "GET /test" + ss1.str() + " HTTP/1.1\r\n"
                             "Host: localhost\r\n\r\n";
        req->parseRequest(httpReq);
        delete req;
        
        HttpResponse* resp = new HttpResponse();
        resp->setStatus(200, "OK");
        std::stringstream ss2;
        ss2 << i;
        resp->setBody("Response " + ss2.str());
        std::string responseStr = resp->toString();
        delete resp;
    }
    
    std::cout << "메모리 테스트 완료 (메모리 리크는 valgrind로 확인하세요)" << std::endl;
}

int main() {
    testHttpRequestEdgeCases();
    testHttpResponseEdgeCases();
    testMimeTypeDetection();
    performanceTest();
    memoryLeakTest();
    
    std::cout << "\n=== 추가 테스트 완료 ===" << std::endl;
    std::cout << "valgrind --leak-check=full ./test_program 으로 메모리 리크를 확인하세요." << std::endl;
    
    return 0;
}