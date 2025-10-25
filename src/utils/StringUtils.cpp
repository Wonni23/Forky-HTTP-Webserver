// src/utils/StringUtils.cpp
#include "utils/StringUtils.hpp"
#include <sstream>
#include <cctype>
#include <algorithm>

namespace StringUtils {

// 공백 문자 정의
static const std::string WHITESPACE = " \t\n\r\f\v";

// ========= 🔥 Trim 함수 구현 ==========

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

// ========= 기존 toBytes 함수 ==========

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

} // namespace StringUtils

