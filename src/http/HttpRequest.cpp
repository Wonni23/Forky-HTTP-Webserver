#include "http/HttpRequest.hpp"
#include "http/StatusCode.hpp"

HttpRequest::HttpRequest() : _isComplete(false), _lastError(PARSE_SUCCESS), _body_file_path("") {}

HttpRequest::~HttpRequest() {}

bool HttpRequest::parseHeaders(const std::string& headerPart) {
    // 헤더 크기 검증
    if (headerPart.length() > MAX_HEADER_SIZE) {
        _lastError = PARSE_HEADER_TOO_LARGE;
        return false;
    }
    
    std::istringstream iss(headerPart);
    std::string line;
    
    // 첫 번째 줄: Request Line 파싱
    if (!std::getline(iss, line)) {
        _lastError = PARSE_INVALID_REQUEST_LINE;
        return false;
    }
    
    // \r 제거
    if (!line.empty() && line[line.length()-1] == '\r') {
        line.erase(line.length()-1);
    }
    
    if (!parseRequestLine(line)) {
        // parseRequestLine에서 이미 에러 설정됨
        return false;
    }
    
    // 나머지 줄들: 헤더 파싱
    int headerCount = 0;
    while (std::getline(iss, line)) {
        // \r 제거
        if (!line.empty() && line[line.length()-1] == '\r') {
            line.erase(line.length()-1);
        }
        if (line.empty()) break;
        
        // 헤더 개수 제한 (DoS 공격 방지)
        if (++headerCount > MAX_HEADER_COUNT) {
            _lastError = PARSE_TOO_MANY_HEADERS;
            return false;
        }
        
        // "Key: Value" 형태 파싱
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            _lastError = PARSE_INVALID_HEADER_FORMAT;
            return false;
        }
        
        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);
        
        // 공백 제거
        key = trim(key);
        value = trim(value);
        
        // 헤더 키 검증
        if (key.empty()) {
            _lastError = PARSE_EMPTY_HEADER_KEY;
            return false;
        }
        
        // 헤더 키 길이 제한
        if (key.length() > MAX_HEADER_KEY_LENGTH) {
            _lastError = PARSE_HEADER_KEY_TOO_LONG;
            return false;
        }
        
        // 헤더 값 길이 제한
        if (value.length() > MAX_HEADER_VALUE_LENGTH) {
            _lastError = PARSE_HEADER_VALUE_TOO_LONG;
            return false;
        }
        
        // 헤더 키는 대소문자 구분 없음
        _headers[toLowerCase(key)] = value;
    }

    // Cookie 헤더 파싱
    parseCookies();

    _lastError = PARSE_SUCCESS;
    return true;
}

bool HttpRequest::parseRequestLine(const std::string& line) {
    // Request Line 길이 제한
    if (line.length() > MAX_REQUEST_LINE_LENGTH) {
        _lastError = PARSE_REQUEST_LINE_TOO_LONG;
        return false;
    }
    
    std::istringstream iss(line);
    
    // "GET /index.html HTTP/1.1" 파싱
    if (!(iss >> _method >> _uri >> _version)) {
        _lastError = PARSE_INVALID_REQUEST_LINE;
        return false;
    }
    
    // 추가 토큰이 있는지 확인 (잘못된 형식)
    std::string extra;
    if (iss >> extra) {
        _lastError = PARSE_INVALID_REQUEST_LINE;
        return false;
    }
    
    // 메서드 검증
    if (_method.empty() || _method.length() > MAX_METHOD_LENGTH) {
        _lastError = PARSE_INVALID_METHOD;
        return false;
    }
    
    if (_method != "GET" && _method != "POST" && _method != "DELETE" && _method != "HEAD") {
        _lastError = PARSE_UNSUPPORTED_METHOD;
        return false;
    }
    
    // URI 검증
    if (_uri.empty() || _uri.length() > MAX_URI_LENGTH) {
        _lastError = PARSE_INVALID_URI;
        return false;
    }
    
    if (_uri[0] != '/') {
        _lastError = PARSE_INVALID_URI;
        return false;
    }
    
    // URL 디코딩 적용
    std::string decodedUri = urlDecode(_uri);
    if (decodedUri.empty()) {
        _lastError = PARSE_INVALID_URI_ENCODING;
        return false;
    }
    _uri = decodedUri;
    
    // 버전 검증
    if (_version != "HTTP/1.1" && _version != "HTTP/1.0") {
        _lastError = PARSE_UNSUPPORTED_VERSION;
        return false;
    }
    
    _lastError = PARSE_SUCCESS;
    return true;
}

std::string HttpRequest::urlDecode(const std::string& str) const {
    std::string result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            // %XX 형태를 ASCII 문자로 변환
            char hex[3] = {str[i+1], str[i+2], '\0'};
            char* endPtr;
            int ascii = std::strtol(hex, &endPtr, 16);
            
            if (*endPtr == '\0' && ascii >= 0 && ascii <= 255) {
                // 제어 문자 필터링 (보안)
                if (ascii < 32 && ascii != 9 && ascii != 10 && ascii != 13) {
                    return ""; // 잘못된 인코딩으로 처리
                }
                result += static_cast<char>(ascii);
                i += 2;
            } else {
                return ""; // 잘못된 인코딩
            }
        } else if (str[i] == '+') {
            result += ' '; // + 는 공백으로 변환
        } else if (static_cast<unsigned char>(str[i]) < 32) {
            // 제어 문자 필터링
            return "";
        } else {
            result += str[i];
        }
    }
    
    return result;
}

std::string HttpRequest::trim(const std::string& str) const {
    const std::string whitespace = " \t\r\n";
    
    size_t first = str.find_first_not_of(whitespace);
    if (first == std::string::npos) return "";
    
    size_t last = str.find_last_not_of(whitespace);
    return str.substr(first, (last - first + 1));
}

std::string HttpRequest::toLowerCase(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

bool HttpRequest::parseRequest(const std::string& completeHttpRequest) {
    if (_lastError != PARSE_INCOMPLETE) {
        reset();
    }

    // 전체 요청 크기 검증
    if (completeHttpRequest.length() > MAX_REQUEST_SIZE) {
        _lastError = PARSE_REQUEST_TOO_LARGE;
        return false;
    }

    // 1. 헤더와 바디 분리
    size_t headerEnd = completeHttpRequest.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        _lastError = PARSE_INCOMPLETE;
        return false;
    }

    std::string headerPart = completeHttpRequest.substr(0, headerEnd);
    std::string bodyPart = completeHttpRequest.substr(headerEnd + 4);

    // 2. 헤더 파싱
    if (!parseHeaders(headerPart)) {
        return false;
    }

    std::cout << "[DEBUG] [HttpRequest] After parseHeaders: method=" << _method
              << ", hasContentLength=" << (hasHeader("content-length") ? "YES" : "NO") << std::endl;

    // 4. 바디 처리 및 완료 상태 결정
    bool bodyComplete = false;

    bool isChunked = isChunkedEncoding();
    std::cout << "[DEBUG] [HttpRequest] isChunkedEncoding=" << (isChunked ? "YES" : "NO") << std::endl;

    if (isChunked) {
        // ===== CHUNKED ENCODING 처리 =====
        _body = decodeChunkedBody(bodyPart);
        
        if (!_body.empty()) {
            bodyComplete = true;
        } else {
            if (bodyPart.empty()) {
                _lastError = PARSE_INCOMPLETE;
                return false;
            }

            if (bodyPart.length() >= 5 &&
                bodyPart.substr(0, 5) == "0\r\n\r\n") {
                std::cout << "[DEBUG] [HttpRequest] Chunked: Empty body detected" << std::endl;
                bodyComplete = true;
                _body = "";
            } else {
                _lastError = PARSE_INCOMPLETE;
                return false;
            }
        }
    } else if (_method == "POST" || _method == "PUT") {
        // ===== NON-CHUNKED (Content-Length 기반) 처리 =====
        size_t expectedLength = getContentLength();
        
        std::cout << "[DEBUG] [HttpRequest] expectedLength=" << expectedLength
                  << ", bodyPart.length()=" << bodyPart.length() << std::endl;
        
        if (expectedLength > 0) {
            if (bodyPart.length() < expectedLength) {
                _lastError = PARSE_INCOMPLETE;
                return false;
            } else if (bodyPart.length() > expectedLength) {
                _lastError = PARSE_BODY_LENGTH_MISMATCH;
                return false;
            }
            _body = bodyPart;
            bodyComplete = true;
            
        } else {
            if (hasHeader("content-length")) {
                _body = bodyPart;
                if (!bodyPart.empty()) {
                    _lastError = PARSE_BODY_LENGTH_MISMATCH;
                    return false;
                }
                bodyComplete = true;
            } else {
                if (!bodyPart.empty()) {
                    _lastError = PARSE_BODY_LENGTH_MISMATCH;
                    return false;
                }

                _body = "";
                bodyComplete = true;
            }
        }
    } else {
        // GET/DELETE/etc. 요청
        std::cout << "[DEBUG] [HttpRequest] Entered GET/DELETE/etc path, method=" << _method << std::endl;
        _body = bodyPart;
        bodyComplete = true;
    }
    
    // 5. Multipart form data 파싱
    if (isMultipartFormData() && !_body.empty()) {
        std::string contentType = getHeader("content-type");
        size_t boundaryPos = contentType.find("boundary=");
        if (boundaryPos != std::string::npos) {
            boundaryPos += 9;
            size_t boundaryEnd = contentType.find(';', boundaryPos);
            if (boundaryEnd == std::string::npos) {
                boundaryEnd = contentType.length();
            }
            std::string boundary = contentType.substr(boundaryPos, boundaryEnd - boundaryPos);
            boundary = trim(boundary);

            if (boundary.length() >= 2 && boundary[0] == '"' && boundary[boundary.length()-1] == '"') {
                boundary = boundary.substr(1, boundary.length()-2);
            }

            parseMultipartData(_body, boundary);
        }
    }

    if (bodyComplete) {
        _isComplete = true;
        _lastError = PARSE_SUCCESS;
        return true;
    }

    _lastError = PARSE_INCOMPLETE;
    return false;
}


bool HttpRequest::parseHeadersOnly(const std::string& headerPart) {
    // 임시 상태 저장
    std::string oldMethod = _method;
    std::string oldUri = _uri;
    std::string oldVersion = _version;
    std::map<std::string, std::string> oldHeaders = _headers;
    
    // 헤더 파싱 시도
    bool result = parseHeaders(headerPart);
    
    // 실패 시 상태 복원
    if (!result) {
        _method = oldMethod;
        _uri = oldUri;
        _version = oldVersion;
        _headers = oldHeaders;
    }
    
    return result;
}

const std::string& HttpRequest::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(toLowerCase(key));
    if (it != _headers.end()) {
        return it->second;
    }
    static const std::string empty = "";
    return empty;
}

bool HttpRequest::hasHeader(const std::string& key) const {
    return _headers.find(toLowerCase(key)) != _headers.end();
}

size_t HttpRequest::getContentLength() const {
    if (hasHeader("content-length")) {
        const std::string& lengthStr = getHeader("content-length");
        char* endPtr;
        // Content-Length는 음수가 될 수 없으므로, unsigned long을 사용하는 std::strtoul로 변경합니다.
        // 이는 대용량 크기 파싱의 안정성을 높입니다.
        unsigned long length = std::strtoul(lengthStr.c_str(), &endPtr, 10);
        
        // 숫자 검증: 문자열 끝까지 완전히 변환되지 않은 경우 (잘못된 문자 포함)
        if (*endPtr != '\0') {
            return 0;
        }
        
        return static_cast<size_t>(length);
    }
    return 0;
}

bool HttpRequest::isChunkedEncoding() const {
    std::string encoding = getHeader("transfer-encoding");
    return toLowerCase(encoding) == "chunked";
}

bool HttpRequest::isRequestTooLarge(size_t size) const {
    return size > MAX_REQUEST_SIZE;
}

bool HttpRequest::isValidRequest() const {
    return _isComplete && _lastError == PARSE_SUCCESS;
}

bool HttpRequest::isKeepAlive() const {
    std::string connectionheader = toLowerCase(getHeader("connection"));

    // For HTTP/1.1: default value is "Keep-Alive"
    // return false only when "Connection: close" is specified
    if (_version == "HTTP/1.1") {
        return (connectionheader != "close");
    }

    // For HTTP/1.0: default value is "Closed"
    // return false onlt when "Connection: keep-alive" is specified
    if (_version == "HTTP/1.0") {
        return (connectionheader != "keep-alive");
    }

    // For any other cases (including errors), default to closeing the connection for safety
    return (false);
}

const char* HttpRequest::getErrorMessage() const {
    switch (_lastError) {
        case PARSE_SUCCESS:
            return "Success";
        case PARSE_INCOMPLETE:
            return "Request incomplete";
        case PARSE_REQUEST_TOO_LARGE:
            return "Request too large";
        case PARSE_HEADER_TOO_LARGE:
            return "Header too large";
        case PARSE_BODY_TOO_LARGE:
            return "Body too large";
        case PARSE_INVALID_REQUEST_LINE:
            return "Invalid request line";
        case PARSE_REQUEST_LINE_TOO_LONG:
            return "Request line too long";
        case PARSE_INVALID_METHOD:
            return "Invalid method";
        case PARSE_UNSUPPORTED_METHOD:
            return "Unsupported method";
        case PARSE_INVALID_URI:
            return "Invalid URI";
        case PARSE_INVALID_URI_ENCODING:
            return "Invalid URI encoding";
        case PARSE_UNSUPPORTED_VERSION:
            return "Unsupported HTTP version";
        case PARSE_INVALID_HEADER_FORMAT:
            return "Invalid header format";
        case PARSE_EMPTY_HEADER_KEY:
            return "Empty header key";
        case PARSE_HEADER_KEY_TOO_LONG:
            return "Header key too long";
        case PARSE_HEADER_VALUE_TOO_LONG:
            return "Header value too long";
        case PARSE_TOO_MANY_HEADERS:
            return "Too many headers";
        case PARSE_BODY_LENGTH_MISMATCH:
            return "Body length mismatch";
        default:
            return "Unknown error";
    }
}

int HttpRequest::getStatusCodeForError() const {
    switch (_lastError) 
    {
        case PARSE_REQUEST_TOO_LARGE:
        case PARSE_HEADER_TOO_LARGE:
        case PARSE_BODY_TOO_LARGE:
            return StatusCode::PAYLOAD_TOO_LARGE;           // 413

        case PARSE_UNSUPPORTED_METHOD:
            return StatusCode::NOT_IMPLEMENTED;             // 501

        case PARSE_UNSUPPORTED_VERSION:
            return StatusCode::HTTP_VERSION_NOT_SUPPORTED;  // 505

        case PARSE_INVALID_URI:
        case PARSE_REQUEST_LINE_TOO_LONG:
            return StatusCode::URI_TOO_LONG;                // 414

        case PARSE_INVALID_REQUEST_LINE:
        case PARSE_INVALID_METHOD:
        case PARSE_INVALID_URI_ENCODING:
        case PARSE_INVALID_HEADER_FORMAT:
        case PARSE_EMPTY_HEADER_KEY:
        case PARSE_HEADER_KEY_TOO_LONG:
        case PARSE_HEADER_VALUE_TOO_LONG:
        case PARSE_TOO_MANY_HEADERS:
        case PARSE_BODY_LENGTH_MISMATCH:
        default:
            return StatusCode::BAD_REQUEST;                // 400
    }
}

std::string HttpRequest::decodeChunkedBody(const std::string& chunkedBody) const {
    std::string result;
    size_t pos = 0;
    bool foundLastChunk = false;

    std::cout << "[DEBUG] [decodeChunkedBody] Starting to decode, total raw size: " 
              << chunkedBody.length() << " bytes" << std::endl;

    while (pos < chunkedBody.length()) {
        // 청크 크기 읽기 (헥사값)
        size_t crlfPos = chunkedBody.find("\r\n", pos);
        if (crlfPos == std::string::npos) {
            // std::cout << "[DEBUG] [decodeChunkedBody] No CRLF found at pos " << pos << std::endl;
            return ""; // 잘못된 형식
        }

        std::string chunkSizeStr = chunkedBody.substr(pos, crlfPos - pos);
        // std::cout << "[DEBUG] [decodeChunkedBody] Chunk size string: '" << chunkSizeStr << "'" << std::endl;

        // 세미콜론이 있다면 청크 확장을 무시
        size_t semicolonPos = chunkSizeStr.find(';');
        if (semicolonPos != std::string::npos) {
            chunkSizeStr = chunkSizeStr.substr(0, semicolonPos);
        }

        // 헥사값을 정수로 변환
        char* endPtr;
        long chunkSize = std::strtol(chunkSizeStr.c_str(), &endPtr, 16);

        if (*endPtr != '\0' || chunkSize < 0) {
            // std::cout << "[DEBUG] [decodeChunkedBody] Invalid chunk size: '" << chunkSizeStr << "'" << std::endl;
            return ""; // 잘못된 청크 크기
        }

        // std::cout << "[DEBUG] [decodeChunkedBody] Parsed chunk size: " << chunkSize << " bytes (0x" << chunkSizeStr << ")" << std::endl;

        if (chunkSize == 0) {
            // 마지막 청크 (trailer headers 무시)
            std::cout << "[DEBUG] [decodeChunkedBody] Last chunk found, total decoded: " 
                      << result.size() << " bytes" << std::endl;
            foundLastChunk = true;
            break;
        }

        pos = crlfPos + 2; // CRLF 건너뛰기

        // 청크 데이터 읽기
        if (pos + chunkSize > chunkedBody.length()) {
            // std::cout << "[DEBUG] [decodeChunkedBody] Insufficient data: need " << (pos + chunkSize) << " bytes, have " << chunkedBody.length() << std::endl;
            return ""; // 데이터 부족
        }

        result.append(chunkedBody.substr(pos, chunkSize));
        pos += chunkSize;

        // 청크 끝의 CRLF 확인
        if (pos + 2 > chunkedBody.length() ||
            chunkedBody.substr(pos, 2) != "\r\n") {
            // std::cout << "[DEBUG] [decodeChunkedBody] Missing CRLF after chunk data at pos " << pos << std::endl;
            return ""; // 잘못된 형식
        }

        pos += 2; // CRLF 건너뛰기
    }

    // 마지막 청크(0\r\n\r\n)를 찾지 못하고 while 루프가 끝났다면 데이터 불완전
    if (!foundLastChunk) {
        // std::cout << "[DEBUG] [decodeChunkedBody] Last chunk not found, data incomplete" << std::endl;
        return ""; // 데이터 불완전
    }

    return result;
}

bool HttpRequest::parseMultipartData(const std::string& body, const std::string& boundary) {
    _formFields.clear();

    std::string delimiter = "--" + boundary;
    std::string endDelimiter = "--" + boundary + "--";

    size_t pos = 0;

    // 첫 번째 경계 찾기
    pos = body.find(delimiter, pos);
    if (pos == std::string::npos) {
        return false;
    }
    pos += delimiter.length();

    while (pos < body.length()) {
        // CRLF 건너뛰기
        if (pos + 2 <= body.length() && body.substr(pos, 2) == "\r\n") {
            pos += 2;
        }

        // 다음 경계 또는 종료 경계 찾기
        size_t nextBoundary = body.find(delimiter, pos);
        if (nextBoundary == std::string::npos) {
            break;
        }

        std::string part = body.substr(pos, nextBoundary - pos);

        // 파트 헤더와 데이터 분리
        size_t headerEnd = part.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            continue; // 잘못된 파트
        }

        std::string partHeaders = part.substr(0, headerEnd);
        std::string partData = part.substr(headerEnd + 4);

        // 끝의 CRLF 제거
        if (partData.length() >= 2 && partData.substr(partData.length() - 2) == "\r\n") {
            partData = partData.substr(0, partData.length() - 2);
        }

        // Content-Disposition 파싱
        FormField field;
        field.isFile = false;

        // 간단한 Content-Disposition 파싱
        size_t namePos = partHeaders.find("name=\"");
        if (namePos != std::string::npos) {
            namePos += 6; // "name=\"" 길이
            size_t nameEnd = partHeaders.find("\"", namePos);
            if (nameEnd != std::string::npos) {
                field.name = partHeaders.substr(namePos, nameEnd - namePos);
            }
        }

        size_t filenamePos = partHeaders.find("filename=\"");
        if (filenamePos != std::string::npos) {
            filenamePos += 10; // "filename=\"" 길이
            size_t filenameEnd = partHeaders.find("\"", filenamePos);
            if (filenameEnd != std::string::npos) {
                field.filename = partHeaders.substr(filenamePos, filenameEnd - filenamePos);
                field.isFile = true;
            }
        }

        // Content-Type 파싱
        size_t ctPos = partHeaders.find("Content-Type:");
        if (ctPos != std::string::npos) {
            ctPos += 13; // "Content-Type:" 길이
            size_t ctEnd = partHeaders.find("\r\n", ctPos);
            if (ctEnd == std::string::npos) {
                ctEnd = partHeaders.length();
            }
            field.contentType = trim(partHeaders.substr(ctPos, ctEnd - ctPos));
        }

        field.value = partData;
        _formFields.push_back(field);

        pos = nextBoundary + delimiter.length();

        // 종료 경계 확인
        if (pos + 2 <= body.length() && body.substr(pos, 2) == "--") {
            break; // 종료 경계 발견
        }
    }

    return true;
}

bool HttpRequest::isMultipartFormData() const {
    std::string contentType = getHeader("content-type");
    return contentType.find("multipart/form-data") != std::string::npos;
}

const HttpRequest::FormField* HttpRequest::getFormField(const std::string& name) const {
    for (size_t i = 0; i < _formFields.size(); ++i) {
        if (_formFields[i].name == name) {
            return &_formFields[i];
        }
    }
    return NULL;
}

void HttpRequest::reset() {
    _method.clear();
    _uri.clear();
    _version.clear();
    _headers.clear();
    _body.clear();
    _formFields.clear();
    _cookies.clear();
    _isComplete = false;
    _lastError = PARSE_SUCCESS;
}

// ============================================================================
// Cookie 파싱
// ============================================================================
void HttpRequest::parseCookies() {
    _cookies.clear();

    std::string cookieHeader = getHeader("cookie");
    if (cookieHeader.empty()) {
        return;
    }

    // Cookie 헤더 형식: "name1=value1; name2=value2; name3=value3"
    std::istringstream iss(cookieHeader);
    std::string cookiePair;

    while (std::getline(iss, cookiePair, ';')) {
        // 앞뒤 공백 제거
        cookiePair = trim(cookiePair);

        // "name=value" 파싱
        size_t equalPos = cookiePair.find('=');
        if (equalPos == std::string::npos) {
            continue; // 형식이 잘못된 쿠키는 무시
        }

        std::string name = trim(cookiePair.substr(0, equalPos));
        std::string value = trim(cookiePair.substr(equalPos + 1));

        if (!name.empty()) {
            _cookies[name] = value;
        }
    }
}

const std::string& HttpRequest::getCookie(const std::string& name) const {
    static const std::string emptyString = "";
    std::map<std::string, std::string>::const_iterator it = _cookies.find(name);

    if (it != _cookies.end()) {
        return it->second;
    }
    return emptyString;
}

bool HttpRequest::hasCookie(const std::string& name) const {
    return _cookies.find(name) != _cookies.end();
}
