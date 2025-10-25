// include/utils/StringUtils.hpp
#ifndef STRINGUTILS_HPP
#define STRINGUTILS_HPP

#include <string>
#include <cstddef> // size_t 사용

namespace StringUtils {

    /**
     * @brief 크기 단위 문자열(ex: "10M", "512K")을 바이트 단위로 변환.
     *
     * @param sizeStr 변환할 크기 단위 문자열. (K, M, G 단위 지원)
     * @return 변환된 바이트 크기를 size_t 타입으로 반환.
     *         단위가 없거나 알 수 없는 단위일 경우 숫자 부분만 반환함.
     *         입력 문자열이 비어있으면 0을 반환함.
     */
    size_t toBytes(const std::string& sizeStr);
    
    /**
     * @brief 🆕 문자열 양쪽의 공백 제거
     *
     * @param s 원본 문자열
     * @return 공백이 제거된 새 문자열
     */
    std::string trim(const std::string& s);
    
    /**
     * @brief 🆕 문자열 왼쪽 공백 제거
     *
     * @param s 원본 문자열
     * @return 왼쪽 공백이 제거된 새 문자열
     */
    std::string trimLeft(const std::string& s);
    
    /**
     * @brief 🆕 문자열 오른쪽 공백 제거
     *
     * @param s 원본 문자열
     * @return 오른쪽 공백이 제거된 새 문자열
     */
    std::string trimRight(const std::string& s);

} // namespace StringUtils

#endif

