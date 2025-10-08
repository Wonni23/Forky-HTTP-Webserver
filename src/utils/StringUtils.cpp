#include "StringUtils.hpp"
#include <cctype> // isdigit, toupper 함수 사용
#include <cstdint>

namespace StringUtils {

	size_t toBytes(const std::string& sizeStr) {
		if (sizeStr.empty()) {
			// 입력이 없으면 0 바이트로 처리.
			return 0;
		}

		size_t num_part = 0;      // 숫자 부분 저장.
		std::string unit_part;    // 단위 부분(K, M, G) 저장.

		// 문자열 시작부터 순회할 이터레이터 선언.
		std::string::const_iterator it = sizeStr.begin();

		// 문자열에서 숫자 부분(digit) 파싱.
		while (it != sizeStr.end() && std::isdigit(*it)) {
			// size_t의 최대 값을 초과하지 않도록 오버플로우 방지.
			if (num_part > (SIZE_MAX / 10)) {
				num_part = SIZE_MAX;
				break;
			}
			num_part = num_part * 10 + (*it - '0');
			++it;
		}

		// 숫자 파싱이 끝난 지점부터 문자열 끝까지를 단위 부분으로 간주.
		if (it != sizeStr.end()) {
			unit_part = std::string(it, sizeStr.end());
		}

		// 단위 부분에 따라 최종 바이트 값 계산.
		if (!unit_part.empty()) {
			// 대소문자 구분을 없애기 위해 단위를 대문자로 변경.
			char unit = std::toupper(unit_part[0]);
			switch (unit) {
				case 'K': // 킬로바이트.
					return num_part * 1024;
				case 'M': // 메가바이트.
					return num_part * 1024 * 1024;
				case 'G': // 기가바이트.
					return num_part * 1024 * 1024 * 1024;
				default:
					// K, M, G가 아닌 알 수 없는 단위는 무시하고 숫자 부분만 반환.
					return num_part;
			}
		}

		// 단위 부분이 없으면 숫자 부분만 그대로 반환.
		return num_part;
	}

} // namespace StringUtils

