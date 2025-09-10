#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

class HttpRequest {
public:
    /* 에러 코드 정의 */
    enum ParseError {
        PARSE_SUCCESS,
        PARSE_INCOMPLETE,
        PARSE_REQUEST_TOO_LARGE,
        PARSE_HEADER_TOO_LARGE,
        PARSE_BODY_TOO_LARGE,
        PARSE_INVALID_REQUEST_LINE,
        PARSE_REQUEST_LINE_TOO_LONG,
        PARSE_INVALID_METHOD,
        PARSE_UNSUPPORTED_METHOD,
        PARSE_INVALID_URI,
        PARSE_INVALID_URI_ENCODING,
        PARSE_UNSUPPORTED_VERSION,
        PARSE_INVALID_HEADER_FORMAT,
        PARSE_EMPTY_HEADER_KEY,
        PARSE_HEADER_KEY_TOO_LONG,
        PARSE_HEADER_VALUE_TOO_LONG,
        PARSE_TOO_MANY_HEADERS,
        PARSE_BODY_LENGTH_MISMATCH
    };

private:
    /* 크기 제한 상수들 */
    static const size_t MAX_REQUEST_SIZE = 1024 * 1024;      // 1MB
    static const size_t MAX_HEADER_SIZE = 8192;              // 8KB
    static const size_t MAX_BODY_SIZE = 10 * 1024 * 1024;    // 10MB
    static const size_t MAX_REQUEST_LINE_LENGTH = 2048;      // 2KB
    static const size_t MAX_METHOD_LENGTH = 16;              // 16 bytes
    static const size_t MAX_URI_LENGTH = 2000;               // 2000 bytes
    static const size_t MAX_HEADER_KEY_LENGTH = 256;         // 256 bytes
    static const size_t MAX_HEADER_VALUE_LENGTH = 4096;      // 4KB
    static const int MAX_HEADER_COUNT = 100;                 // 100 headers

    /* 멤버 변수 */
    std::string _method;          // GET, POST, DELETE
    std::string _uri;             // /index.html
    std::string _version;         // HTTP/1.1
    std::map<std::string, std::string> _headers;
    std::string _body;
    bool _isComplete;             // 파싱 완료 여부
    ParseError _lastError;        // 마지막 에러 정보

    /* 파싱 관련 private 함수 */
    bool parseHeaders(const std::string& headerPart);
    bool parseRequestLine(const std::string& line);
    std::string urlDecode(const std::string& str) const;

    /* 내부 유틸리티 함수 */
    std::string trim(const std::string& str) const;
    std::string toLowerCase(const std::string& str) const;

public:
    HttpRequest();
    ~HttpRequest();

    /* 파싱 관련 함수 */
    bool parseRequest(const std::string& completeHttpRequest);  // 완성된 HTTP 요청 파싱 (Client에서 모든 처리 완료 후 호출)
    bool parseHeadersOnly(const std::string& headerPart);       // 헤더만 파싱 (Client에서 청크 여부 확인용)

    /* Getter 함수 */
    const std::string& getMethod() const { return _method; }
    const std::string& getUri() const { return _uri; }
    const std::string& getVersion() const { return _version; }
    const std::string& getHeader(const std::string& key) const;
    const std::string& getBody() const { return _body; }

    /* 상태 확인 */
    bool isComplete() const { return _isComplete; }
    bool isValidRequest() const;

    /* 에러 처리 */
    ParseError getLastError() const { return _lastError; }
    const char* getErrorMessage() const;

    /* 유틸리티 */
    bool hasHeader(const std::string& key) const;
    size_t getContentLength() const;
    bool isChunkedEncoding() const;
    bool isRequestTooLarge(size_t size) const;

    /* 재사용을 위한 초기화 */
    void reset();
};

#endif // HTTP_REQUEST_HPP