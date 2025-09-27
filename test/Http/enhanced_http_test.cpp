#include "HttpRequest.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <cassert>

class EnhancedHttpTest {
private:
    int totalTests;
    int passedTests;

    bool runTest(const std::string& testName, bool condition) {
        totalTests++;
        if (condition) {
            passedTests++;
            std::cout << "[PASS] " << testName << std::endl;
            return true;
        } else {
            std::cout << "[FAIL] " << testName << std::endl;
            return false;
        }
    }

public:
    EnhancedHttpTest() : totalTests(0), passedTests(0) {}

    void testChunkedDecoding() {
        std::cout << "\n=== Chunked Transfer Encoding Tests ===" << std::endl;

        HttpRequest request;

        // Test basic chunked request
        std::string chunkedRequest =
            "POST /upload HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "7\r\n"
            "Mozilla\r\n"
            "9\r\n"
            "Developer\r\n"
            "7\r\n"
            "Network\r\n"
            "0\r\n"
            "\r\n";

        runTest("Parse chunked request",
                request.parseRequest(chunkedRequest));
        runTest("Chunked body decoded correctly",
                request.getBody() == "MozillaDeveloperNetwork");

        // Test chunked with chunk extensions
        request.reset();
        std::string chunkedWithExtensions =
            "POST /data HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "5;metadata=test\r\n"
            "Hello\r\n"
            "6;info=data\r\n"
            " World\r\n"
            "0\r\n"
            "\r\n";

        runTest("Parse chunked with extensions",
                request.parseRequest(chunkedWithExtensions));
        runTest("Chunked with extensions body",
                request.getBody() == "Hello World");

        // Test empty chunks
        request.reset();
        std::string emptyChunked =
            "POST /empty HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "0\r\n"
            "\r\n";

        runTest("Parse empty chunked request",
                request.parseRequest(emptyChunked));
        runTest("Empty chunked body",
                request.getBody() == "");

        // Test invalid chunk size
        request.reset();
        std::string invalidChunked =
            "POST /invalid HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "XYZ\r\n"
            "data\r\n"
            "0\r\n"
            "\r\n";

        runTest("Reject invalid chunk size",
                !request.parseRequest(invalidChunked));
    }

    void testMultipartParsing() {
        std::cout << "\n=== Multipart Form Data Tests ===" << std::endl;

        HttpRequest request;

        // Test basic multipart form
        std::string boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
        std::string multipartBody =
            "--" + boundary + "\r\n"
            "Content-Disposition: form-data; name=\"username\"\r\n"
            "\r\n"
            "john_doe\r\n"
            "--" + boundary + "\r\n"
            "Content-Disposition: form-data; name=\"email\"\r\n"
            "\r\n"
            "john@example.com\r\n"
            "--" + boundary + "\r\n"
            "Content-Disposition: form-data; name=\"file\"; filename=\"test.txt\"\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "Hello World!\r\n"
            "--" + boundary + "--\r\n";

        std::ostringstream oss;
        oss << multipartBody.length();

        std::string multipartRequest =
            "POST /upload HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
            "Content-Length: " + oss.str() + "\r\n"
            "\r\n" + multipartBody;

        runTest("Parse multipart form data",
                request.parseRequest(multipartRequest));
        runTest("Is multipart form data",
                request.isMultipartFormData());

        const std::vector<HttpRequest::FormField>& fields = request.getFormFields();
        runTest("Correct number of form fields",
                fields.size() == 3);

        // Check username field
        const HttpRequest::FormField* usernameField = request.getFormField("username");
        runTest("Username field exists",
                usernameField != NULL);
        if (usernameField) {
            runTest("Username field value",
                    usernameField->value == "john_doe");
            runTest("Username field is not file",
                    !usernameField->isFile);
        }

        // Check email field
        const HttpRequest::FormField* emailField = request.getFormField("email");
        runTest("Email field exists",
                emailField != NULL);
        if (emailField) {
            runTest("Email field value",
                    emailField->value == "john@example.com");
        }

        // Check file field
        const HttpRequest::FormField* fileField = request.getFormField("file");
        runTest("File field exists",
                fileField != NULL);
        if (fileField) {
            runTest("File field value",
                    fileField->value == "Hello World!");
            runTest("File field is file",
                    fileField->isFile);
            runTest("File field filename",
                    fileField->filename == "test.txt");
            runTest("File field content type",
                    fileField->contentType == "text/plain");
        }
    }

    void testURLDecoding() {
        std::cout << "\n=== URL Decoding Tests ===" << std::endl;

        HttpRequest request;

        // Test basic URL encoding
        std::string encodedRequest =
            "GET /path%20with%20spaces/file%2Etxt HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "\r\n";

        runTest("Parse URL with percent encoding",
                request.parseRequest(encodedRequest));
        runTest("URL decoded correctly",
                request.getUri() == "/path with spaces/file.txt");

        // Test special characters
        request.reset();
        std::string specialCharsRequest =
            "GET /path%21%40%23%24%25%5E%26%2A HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "\r\n";

        runTest("Parse URL with special characters",
                request.parseRequest(specialCharsRequest));
        runTest("Special characters decoded",
                request.getUri() == "/path!@#$%^&*");

        // Test plus sign to space conversion
        request.reset();
        std::string plusRequest =
            "GET /query+with+plus+signs HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "\r\n";

        runTest("Parse URL with plus signs",
                request.parseRequest(plusRequest));
        runTest("Plus signs converted to spaces",
                request.getUri() == "/query with plus signs");

        // Test invalid encoding
        request.reset();
        std::string invalidEncodingRequest =
            "GET /path%ZZ HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "\r\n";

        runTest("Reject invalid URL encoding",
                !request.parseRequest(invalidEncodingRequest));
    }

    void testCombinedFeatures() {
        std::cout << "\n=== Combined Features Tests ===" << std::endl;

        HttpRequest request;

        // Test chunked multipart (complex case)
        std::string boundary = "----FormBoundary123";
        std::string multipartChunkBody =
            "--" + boundary + "\r\n"
            "Content-Disposition: form-data; name=\"data\"\r\n"
            "\r\n"
            "test data\r\n"
            "--" + boundary + "--\r\n";

        // Convert size to hex
        std::ostringstream hexSizeStream;
        hexSizeStream << std::hex << multipartChunkBody.length();
        std::string hexSize = hexSizeStream.str();

        std::string chunkedMultipartRequest =
            "POST /upload HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
            "\r\n" +
            hexSize + "\r\n" +
            multipartChunkBody + "\r\n" +
            "0\r\n"
            "\r\n";

        runTest("Parse chunked multipart request",
                request.parseRequest(chunkedMultipartRequest));
        runTest("Chunked multipart is multipart",
                request.isMultipartFormData());

        const HttpRequest::FormField* dataField = request.getFormField("data");
        runTest("Chunked multipart field exists",
                dataField != NULL);
        if (dataField) {
            runTest("Chunked multipart field value",
                    dataField->value == "test data");
        }
    }

    void testEdgeCases() {
        std::cout << "\n=== Edge Cases Tests ===" << std::endl;

        HttpRequest request;

        // Test empty multipart
        std::string emptyMultipartRequest =
            "POST /upload HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Content-Type: multipart/form-data; boundary=test\r\n"
            "Content-Length: 0\r\n"
            "\r\n";

        runTest("Parse empty multipart request",
                request.parseRequest(emptyMultipartRequest));
        runTest("Empty multipart has no fields",
                request.getFormFields().size() == 0);

        // Test boundary in quoted form
        request.reset();
        std::string quotedBoundaryBody =
            "--test-boundary\r\n"
            "Content-Disposition: form-data; name=\"test\"\r\n"
            "\r\n"
            "value\r\n"
            "--test-boundary--\r\n";

        std::ostringstream oss2;
        oss2 << quotedBoundaryBody.length();

        std::string quotedBoundaryRequest =
            "POST /upload HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Content-Type: multipart/form-data; boundary=\"test-boundary\"\r\n"
            "Content-Length: " + oss2.str() + "\r\n"
            "\r\n" + quotedBoundaryBody;

        runTest("Parse multipart with quoted boundary",
                request.parseRequest(quotedBoundaryRequest));
        runTest("Quoted boundary field exists",
                request.getFormField("test") != NULL);
    }

    void runAllTests() {
        std::cout << "Starting Enhanced HTTP Tests..." << std::endl;

        testChunkedDecoding();
        testMultipartParsing();
        testURLDecoding();
        testCombinedFeatures();
        testEdgeCases();

        std::cout << "\n=== Test Results ===" << std::endl;
        std::cout << "Passed: " << passedTests << "/" << totalTests << std::endl;

        if (passedTests == totalTests) {
            std::cout << "All enhanced HTTP tests passed! ✅" << std::endl;
        } else {
            std::cout << "Some enhanced HTTP tests failed! ❌" << std::endl;
        }
    }
};

int main() {
    EnhancedHttpTest test;
    test.runAllTests();
    return 0;
}