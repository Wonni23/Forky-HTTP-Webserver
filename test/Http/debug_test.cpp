#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <iostream>
#include <string>

void debugPostRequest() {
    std::cout << "=== POST 요청 디버깅 ===" << std::endl;
    
    HttpRequest req;
    std::string httpReq = 
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: 23\r\n"
        "\r\n"
        "name=test&value=hello";
    
    std::cout << "테스트할 POST 요청:" << std::endl;
    std::cout << "---" << std::endl;
    std::cout << httpReq << std::endl;
    std::cout << "---" << std::endl;
    
    bool success = req.parseRequest(httpReq);
    
    std::cout << "파싱 결과: " << (success ? "성공" : "실패") << std::endl;
    
    if (!success) {
        std::cout << "에러 코드: " << req.getLastError() << std::endl;
        std::cout << "에러 메시지: " << req.getErrorMessage() << std::endl;
    } else {
        std::cout << "메소드: '" << req.getMethod() << "'" << std::endl;
        std::cout << "URI: '" << req.getUri() << "'" << std::endl;
        std::cout << "버전: '" << req.getVersion() << "'" << std::endl;
        std::cout << "Host 헤더: '" << req.getHeader("Host") << "'" << std::endl;
        std::cout << "Content-Type: '" << req.getHeader("Content-Type") << "'" << std::endl;
        std::cout << "Content-Length: " << req.getContentLength() << std::endl;
        std::cout << "바디: '" << req.getBody() << "'" << std::endl;
        std::cout << "바디 길이: " << req.getBody().length() << std::endl;
        std::cout << "완료 상태: " << (req.isComplete() ? "완료" : "미완료") << std::endl;
    }
    
    // 예상 결과와 비교
    std::cout << "\n=== 예상 결과와 비교 ===" << std::endl;
    bool methodOk = req.getMethod() == "POST";
    bool uriOk = req.getUri() == "/upload";
    bool bodyOk = req.getBody() == "name=test&value=hello";
    bool lengthOk = req.getContentLength() == 23;
    
    std::cout << "메소드 검증: " << (methodOk ? "OK" : "FAIL") << std::endl;
    std::cout << "URI 검증: " << (uriOk ? "OK" : "FAIL") << std::endl;
    std::cout << "바디 검증: " << (bodyOk ? "OK" : "FAIL") << std::endl;
    std::cout << "길이 검증: " << (lengthOk ? "OK" : "FAIL") << std::endl;
}

void debugPerformanceRequest() {
    std::cout << "\n=== 성능 테스트 요청 디버깅 ===" << std::endl;
    
    HttpRequest req;
    std::string httpReq = 
        "POST /api/data HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 100\r\n"
        "User-Agent: Test Client\r\n"
        "Accept: application/json\r\n"
        "\r\n"
        "{\"data\": \"test\", \"number\": 12345, \"array\": [1, 2, 3], \"nested\": {\"key\": \"value\"}}";
    
    std::cout << "성능 테스트에서 사용한 요청:" << std::endl;
    std::cout << "---" << std::endl;
    std::cout << httpReq << std::endl;
    std::cout << "---" << std::endl;
    
    std::cout << "실제 바디 길이: " << std::string("{\"data\": \"test\", \"number\": 12345, \"array\": [1, 2, 3], \"nested\": {\"key\": \"value\"}}").length() << std::endl;
    std::cout << "헤더의 Content-Length: 100" << std::endl;
    
    bool success = req.parseRequest(httpReq);
    
    std::cout << "파싱 결과: " << (success ? "성공" : "실패") << std::endl;
    
    if (!success) {
        std::cout << "에러 코드: " << req.getLastError() << std::endl;
        std::cout << "에러 메시지: " << req.getErrorMessage() << std::endl;
    } else {
        std::cout << "파싱 성공!" << std::endl;
        std::cout << "바디: '" << req.getBody() << "'" << std::endl;
        std::cout << "바디 실제 길이: " << req.getBody().length() << std::endl;
        std::cout << "헤더에서 읽은 길이: " << req.getContentLength() << std::endl;
    }
}

void testSimpleRequests() {
    std::cout << "\n=== 간단한 요청들 테스트 ===" << std::endl;
    
    // 1. 가장 간단한 GET
    {
        std::cout << "\n1. 간단한 GET:" << std::endl;
        HttpRequest req;
        std::string httpReq = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
        bool success = req.parseRequest(httpReq);
        std::cout << "결과: " << (success ? "성공" : "실패");
        if (!success) std::cout << " (에러: " << req.getLastError() << ")";
        std::cout << std::endl;
    }
    
    // 2. 바디 없는 POST
    {
        std::cout << "\n2. 바디 없는 POST:" << std::endl;
        HttpRequest req;
        std::string httpReq = 
            "POST /test HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Length: 0\r\n"
            "\r\n";
        bool success = req.parseRequest(httpReq);
        std::cout << "결과: " << (success ? "성공" : "실패");
        if (!success) std::cout << " (에러: " << req.getLastError() << ")";
        std::cout << std::endl;
    }
    
    // 3. 작은 바디 POST
    {
        std::cout << "\n3. 작은 바디 POST:" << std::endl;
        HttpRequest req;
        std::string httpReq = 
            "POST /test HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Length: 4\r\n"
            "\r\n"
            "test";
        bool success = req.parseRequest(httpReq);
        std::cout << "결과: " << (success ? "성공" : "실패");
        if (!success) std::cout << " (에러: " << req.getLastError() << ")";
        if (success) std::cout << " (바디: '" << req.getBody() << "')";
        std::cout << std::endl;
    }
}

void testHeaderParsing() {
    std::cout << "\n=== 헤더 파싱 테스트 ===" << std::endl;
    
    HttpRequest req;
    std::string headerOnly = 
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: 23\r\n"
        "\r\n";
    
    std::cout << "헤더만 파싱:" << std::endl;
    bool success = req.parseHeadersOnly(headerOnly);
    
    std::cout << "결과: " << (success ? "성공" : "실패") << std::endl;
    
    if (success) {
        std::cout << "메소드: '" << req.getMethod() << "'" << std::endl;
        std::cout << "URI: '" << req.getUri() << "'" << std::endl;
        std::cout << "Content-Length: " << req.getContentLength() << std::endl;
        std::cout << "완료 상태: " << (req.isComplete() ? "완료" : "미완료") << std::endl;
    } else {
        std::cout << "에러: " << req.getLastError() << std::endl;
    }
}

int main() {
    debugPostRequest();
    debugPerformanceRequest();
    testSimpleRequests();
    testHeaderParsing();
    
    return 0;
}