// include/utils/StringUtils.hpp
#ifndef STRINGUTILS_HPP
#define STRINGUTILS_HPP

#include <string>
#include <cstddef> // size_t 사용

namespace StringUtils {

    size_t toBytes(const std::string& sizeStr);
    

    std::string trim(const std::string& s);
    std::string trimLeft(const std::string& s);
    std::string trimRight(const std::string& s);

} // namespace StringUtils

#endif