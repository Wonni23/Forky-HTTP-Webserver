#pragma once

#include <string>
#include <map>

/**
 * @brief HTTP 요청 본문(body)에서 파일 파트를 나타내는 구조체
 */
struct FilePart {
    std::string filename; // Content-Disposition에서 추출한 파일명
    std::string content;  // 파일의 실제 바이너리 내용
    bool        isValid;  // 파싱 성공 여부

    FilePart() : isValid(false) {}
};

/**
 * @class MultipartFormDataParser
 * @brief multipart/form-data 본문을 파싱하는 static 유틸리티 클래스
 */
class MultipartFormDataParser {
public:
    /**
     * @brief Content-Type 헤더에서 boundary 문자열을 추출합니다.
     * @param contentTypeHeader (예: "multipart/form-data; boundary=...")
     * @return boundary 문자열 (예: "----WebKitFormBoundary...")
     */
    static std::string getBoundary(const std::string& contentTypeHeader);

    /**
     * @brief multipart 본문을 파싱하여 첫 번째 파일 파트를 추출합니다.
     * @param body 요청 본문 전체
     * @param boundary getBoundary()로 얻은 경계 문자열
     * @return FilePart 구조체. 파일을 못찾으면 isValid == false.
     */
    static FilePart parse(const std::string& body, const std::string& boundary);

private:
    /**
     * @brief Content-Disposition 헤더 라인에서 filename을 추출합니다.
     * @param dispositionLine (예: "form-data; name=\"file\"; filename=\"test.txt\"")
     * @return "test.txt" (실패 시 빈 문자열)
     */
    static std::string getFilename(const std::string& dispositionLine);
};