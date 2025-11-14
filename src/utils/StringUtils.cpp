// src/utils/StringUtils.cpp
#include "utils/StringUtils.hpp"
#include <sstream>
#include <cctype>
#include <algorithm>
#include <iostream> // std::cout, std::cerr, std::endl 사용
#include <fstream>  // std::ifstream (파일 읽기) 사용
#include <string>   // std::string, std::getline 사용

namespace StringUtils {

// 공백 문자 정의
static const std::string WHITESPACE = " \t\n\r\f\v";

std::string trimLeft(const std::string& s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

std::string trimRight(const std::string& s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

std::string trim(const std::string& s)
{
    return trimRight(trimLeft(s));
}

size_t toBytes(const std::string& sizeStr)
{
    if (sizeStr.empty()) {
        return 0;
    }
    
    std::istringstream iss(sizeStr);
    size_t value;
    char unit;
    
    iss >> value;
    
    if (iss >> unit) {
        switch (std::toupper(unit)) {
            case 'K': return value * 1024UL;
            case 'M': return value * 1024UL * 1024UL;
            case 'G': return value * 1024UL * 1024UL * 1024UL;
            default: return value;
        }
    }
    
    return value;
}

void printFileToTerminal(const std::string& filename) {
    // 1. 파일 입력 스트림(ifstream)을 생성하고 파일을 엽니다.
    std::ifstream inputFile(filename.c_str()); // C++98 호환을 위해 .c_str() 사용

    // 2. 파일이 성공적으로 열렸는지 확인합니다.
    if (!inputFile.is_open()) {
        // 파일 열기 실패 시, 표준 오류(stderr)로 메시지를 출력합니다.
        std::cerr << "오류: '" << filename << "' 파일을 열 수 없습니다." << std::endl;
        return; // 함수 종료
    }

    std::string line;
    // 3. 파일의 끝(EOF)에 도달할 때까지 한 줄씩 파일을 읽습니다.
    while (std::getline(inputFile, line)) {
        // 4. 읽어온 한 줄(line)을 표준 출력(stdout)으로 터미널에 출력합니다.
        std::cout << line << std::endl;
    }

    // 5. 파일 스트림을 닫습니다.
    // (사실 inputFile 객체가 소멸될 때 자동으로 닫히지만, 명시적으로 닫는 것이 좋습니다.)
    inputFile.close();
}

} // namespace StringUtils

