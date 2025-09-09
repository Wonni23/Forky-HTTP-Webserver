#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <iostream>
#include <cassert>
#include <string>

// 테스트 헬퍼 함수들
void printSeparator(const std::string& title) {
    std::cout << "\n=== " << title << " ===" << std::endl;
}

void testHttpRequest() {
    printSeparator("HttpRequest 테스트");
    
    // 테스트 1: 기본 GET 요청 파싱
    {
        std::cout << "\n[테스트 1] 기본 GET 요청 파싱" << std::endl;
        
        std::string rawRequest = 
            "GET /index.html HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "User-Agent: Mozilla/5.0\r\n"
            "Accept: text/html\r\n"
            "\r\n";
        
        HttpRequest request;
        bool success = request.parseRequest(rawRequest);
        
        assert(success == true);
        assert(request.getMethod() == "GET");
        assert(request.getUri() == "/index.html");
        assert(request.getVersion() == "HTTP/1.1");
        assert(request.getHeader("host") == "localhost:8080");
        assert(request.getHeader("user-agent") == "Mozilla/5.0");
        assert(request.getBody().empty());
        assert(request.isComplete() == true);
        
        std::cout << "Method: " << request.getMethod() << std::endl;
        std::cout << "URI: " << request.getUri() << std::endl;
        std::cout << "Host: " << request.getHeader("host") << std::endl;
        std::cout << "✅ GET 요청 파싱 성공" << std::endl;
    }
    
    // 테스트 2: POST 요청 (Content-Length)
    {
        std::cout << "\n[테스트 2] POST 요청 (Content-Length)" << std::endl;
        
        std::string rawRequest = 
            "POST /submit HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: 27\r\n"
            "\r\n"
            "name=John&email=john@test.com";
        
        HttpRequest request;
        bool success = request.parseRequest(rawRequest);
        
        assert(success == true);
        assert(request.getMethod() == "POST");
        assert(request.getUri() == "/submit");
        assert(request.getContentLength() == 27);
        assert(request.getBody() == "name=John&email=john@test.com");
        assert(request.isChunkedEncoding() == false);
        
        std::cout << "Method: " << request.getMethod() << std::endl;
        std::cout << "Content-Length: " << request.getContentLength() << std::endl;
        std::cout << "Body: " << request.getBody() << std::endl;
        std::cout << "✅ POST 요청 파싱 성공" << std::endl;
    }
    
    // 테스트 3: URL 디코딩
    {
        std::cout << "\n[테스트 3] URL 디코딩" << std::endl;
        
        std::string rawRequest = 
            "GET /hello%20world?name=John%20Doe HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "\r\n";
        
        HttpRequest request;
        bool success = request.parseRequest(rawRequest);
        
        assert(success == true);
        assert(request.getUri() == "/hello world?name=John Doe");
        
        std::cout << "Original URI: /hello%20world?name=John%20Doe" << std::endl;
        std::cout << "Decoded URI: " << request.getUri() << std::endl;
        std::cout << "✅ URL 디코딩 성공" << std::endl;
    }
    
    // 테스트 4: 청크 인코딩 감지만 테스트
    {
        std::cout << "\n[테스트 4] 청크 인코딩 감지" << std::endl;
        
        std::string headerOnly = 
            "POST /upload HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n";
        
        HttpRequest request;
        bool success = request.parseHeadersOnly(headerOnly);
        
        assert(success == true);
        assert(request.isChunkedEncoding() == true);
        assert(request.getHeader("transfer-encoding") == "chunked");
        
        std::cout << "Transfer-Encoding: " << request.getHeader("transfer-encoding") << std::endl;
        std::cout << "Is Chunked: " << (request.isChunkedEncoding() ? "Yes" : "No") << std::endl;
        std::cout << "✅ 청크 인코딩 감지 성공 (실제 파싱은 Client에서 처리)" << std::endl;
    }
    
    // 테스트 5: 잘못된 요청 처리
    {
        std::cout << "\n[테스트 5] 잘못된 요청 처리" << std::endl;
        
        std::string invalidRequest = "INVALID REQUEST FORMAT";
        
        HttpRequest request;
        bool success = request.parseRequest(invalidRequest);
        
        assert(success == false);
        assert(request.isComplete() == false);
        
        std::cout << "Invalid request handled: " << (success ? "Failed" : "Success") << std::endl;
        std::cout << "✅ 잘못된 요청 처리 성공" << std::endl;
    }
}

void testHttpResponse() {
    printSeparator("HttpResponse 테스트");
    
    // 테스트 1: 기본 200 OK 응답
    {
        std::cout << "\n[테스트 1] 기본 200 OK 응답" << std::endl;
        
        HttpResponse response;
        response.setStatus(200, "OK");
        response.setBody("<h1>Hello World</h1>");
        response.setContentType("text/html");
        response.setContentLength(response.getBody().length());
        response.setDefaultHeaders();
        
        std::string responseStr = response.toString();
        
        assert(response.getStatusCode() == 200);
        assert(response.getStatusMessage() == "OK");
        assert(response.getBody() == "<h1>Hello World</h1>");
        assert(response.getHeader("Content-Type") == "text/html");
        assert(response.getHeader("Server") == "webserv/1.0");
        assert(!response.getHeader("Date").empty());
        
        std::cout << "Status: " << response.getStatusCode() << " " << response.getStatusMessage() << std::endl;
        std::cout << "Content-Type: " << response.getHeader("Content-Type") << std::endl;
        std::cout << "Server: " << response.getHeader("Server") << std::endl;
        std::cout << "✅ 200 OK 응답 생성 성공" << std::endl;
    }
    
    // 테스트 2: 404 에러 응답
    {
        std::cout << "\n[테스트 2] 404 에러 응답" << std::endl;
        
        HttpResponse errorResponse = HttpResponse::createErrorResponse(404);
        
        assert(errorResponse.getStatusCode() == 404);
        assert(errorResponse.getStatusMessage() == "Not Found");
        assert(errorResponse.getHeader("Content-Type") == "text/html");
        assert(!errorResponse.getBody().empty());
        
        std::cout << "Status: " << errorResponse.getStatusCode() << " " << errorResponse.getStatusMessage() << std::endl;
        std::cout << "Content-Type: " << errorResponse.getHeader("Content-Type") << std::endl;
        std::cout << "Body length: " << errorResponse.getBody().length() << std::endl;
        std::cout << "✅ 404 에러 응답 생성 성공" << std::endl;
    }
    
    // 테스트 3: 여러 에러 코드 테스트
    {
        std::cout << "\n[테스트 3] 다양한 에러 코드 테스트" << std::endl;
        
        int testCodes[] = {400, 403, 405, 500, 503};
        std::string expectedMessages[] = {"Bad Request", "Forbidden", "Method Not Allowed", 
                                        "Internal Server Error", "Service Unavailable"};
        
        for (size_t i = 0; i < 5; ++i) {
            HttpResponse response = HttpResponse::createErrorResponse(testCodes[i]);
            assert(response.getStatusCode() == testCodes[i]);
            assert(response.getStatusMessage() == expectedMessages[i]);
            
            std::cout << testCodes[i] << " " << expectedMessages[i] << " ✅" << std::endl;
        }
        
        std::cout << "✅ 다양한 에러 코드 처리 성공" << std::endl;
    }
    
    // 테스트 4: 완전한 HTTP 응답 문자열 검증
    {
        std::cout << "\n[테스트 4] 완전한 HTTP 응답 문자열 검증" << std::endl;
        
        HttpResponse response;
        response.setStatus(200, "OK");
        response.setBody("Test Body");
        response.setContentType("text/plain");
        response.setContentLength(9);
        response.setConnectionHeader("close");
        
        std::string responseStr = response.toString();
        
        // 기본 구조 확인
        assert(responseStr.find("HTTP/1.1 200 OK") != std::string::npos);
        assert(responseStr.find("Content-Type: text/plain") != std::string::npos);
        assert(responseStr.find("Content-Length: 9") != std::string::npos);
        assert(responseStr.find("Connection: close") != std::string::npos);
        assert(responseStr.find("\r\n\r\n") != std::string::npos);
        assert(responseStr.find("Test Body") != std::string::npos);
        
        std::cout << "HTTP 응답 문자열 구조 검증 완료" << std::endl;
        std::cout << "Response preview:" << std::endl;
        std::cout << responseStr.substr(0, 100) << "..." << std::endl;
        std::cout << "✅ HTTP 응답 문자열 검증 성공" << std::endl;
    }
}

void testIntegration() {
    printSeparator("통합 테스트 - 실제 사용 시나리오");
    
    // 시나리오 1: GET 요청 → 200 응답
    {
        std::cout << "\n[시나리오 1] GET 요청 처리" << std::endl;
        
        // 클라이언트 요청
        std::string clientRequest = 
            "GET /about.html HTTP/1.1\r\n"
            "Host: www.example.com\r\n"
            "User-Agent: webserv-test/1.0\r\n"
            "Accept: text/html\r\n"
            "\r\n";
        
        // 요청 파싱
        HttpRequest request;
        bool parseSuccess = request.parseRequest(clientRequest);
        assert(parseSuccess);
        
        // 응답 생성 (파일을 찾았다고 가정)
        HttpResponse response;
        response.setStatus(200, "OK");
        response.setBody("<html><body><h1>About Us</h1></body></html>");
        response.setContentType("text/html");
        response.setContentLength(response.getBody().length());
        response.setDefaultHeaders();
        
        std::cout << "Request: " << request.getMethod() << " " << request.getUri() << std::endl;
        std::cout << "Response: " << response.getStatusCode() << " " << response.getStatusMessage() << std::endl;
        std::cout << "✅ GET 요청 처리 시나리오 성공" << std::endl;
    }
    
    // 시나리오 2: POST 요청 → 파일 업로드
    {
        std::cout << "\n[시나리오 2] POST 파일 업로드" << std::endl;
        
        std::string uploadRequest = 
            "POST /upload HTTP/1.1\r\n"
            "Host: www.example.com\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 11\r\n"
            "\r\n"
            "File content";
        
        HttpRequest request;
        bool parseSuccess = request.parseRequest(uploadRequest);
        assert(parseSuccess);
        
        // 업로드 성공 응답
        HttpResponse response;
        response.setStatus(201, "Created");
        response.setBody("{\"message\": \"File uploaded successfully\"}");
        response.setContentType("application/json");
        response.setContentLength(response.getBody().length());
        response.setDefaultHeaders();
        
        std::cout << "Upload data: " << request.getBody() << std::endl;
        std::cout << "Response: " << response.getStatusCode() << " " << response.getStatusMessage() << std::endl;
        std::cout << "✅ POST 업로드 시나리오 성공" << std::endl;
    }
    
    // 시나리오 3: 잘못된 메서드 → 405 에러
    {
        std::cout << "\n[시나리오 3] 지원하지 않는 메서드" << std::endl;
        
        std::string invalidRequest = 
            "PATCH /api/data HTTP/1.1\r\n"
            "Host: www.example.com\r\n"
            "\r\n";
        
        HttpRequest request;
        bool parseSuccess = request.parseRequest(invalidRequest);
        
        // PATCH는 지원하지 않으므로 파싱 실패
        assert(!parseSuccess);
        
        // 405 에러 응답 생성
        HttpResponse errorResponse = HttpResponse::createErrorResponse(405);
        
        std::cout << "Unsupported method detected" << std::endl;
        std::cout << "Error response: " << errorResponse.getStatusCode() << " " << errorResponse.getStatusMessage() << std::endl;
        std::cout << "✅ 405 에러 처리 시나리오 성공" << std::endl;
    }
}

int main() {
    std::cout << "HttpRequest & HttpResponse 테스트 시작" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        // 개별 컴포넌트 테스트
        testHttpRequest();
        testHttpResponse();
        
        // 통합 테스트
        testIntegration();
        
        printSeparator("모든 테스트 완료");
        std::cout << "🎉 모든 테스트가 성공했습니다!" << std::endl;
        std::cout << "\n이제 Server 클래스에서 다음과 같이 사용할 수 있습니다:" << std::endl;
        std::cout << "1. recv()로 받은 데이터를 HttpRequest::parseRequest()로 파싱" << std::endl;
        std::cout << "2. request.getMethod(), getUri(), getBody() 등으로 요청 정보 확인" << std::endl;
        std::cout << "3. 비즈니스 로직 처리 후 HttpResponse 생성" << std::endl;
        std::cout << "4. response.toString()으로 문자열 변환 후 send()" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "테스트 실패: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "알 수 없는 오류 발생" << std::endl;
        return 1;
    }
    
    return 0;
}