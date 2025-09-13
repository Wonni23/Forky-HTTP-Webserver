# make all          # 컴파일
# make test         # 기본 테스트 실행
# make detailed     # 상세 테스트 실행
# make test-all     # 모든 테스트 실행

# make valgrind              # 기본 테스트 메모리 체크
# make valgrind-detailed     # 상세 테스트 메모리 체크

# make deps         # 의존성 파일들 위치 확인
# make debug        # 디버그 모드로 컴파일
# make check        # 경고를 에러로 처리하지 않고 컴파일

# HTTP 테스트 Makefile

NAME = http_test
DETAILED_TEST = detailed_test
DEBUG_TEST = debug_test

# 컴파일러 및 플래그
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g

# 디렉토리
SRC_DIR = ../../src/http
INCLUDE_DIR = ../../include/http
CURRENT_DIR = .

# 소스 파일들
HTTP_SRCS = $(SRC_DIR)/HttpRequest.cpp $(SRC_DIR)/HttpResponse.cpp
TEST_SRCS = http_test_main.cpp
DETAILED_TEST_SRCS = detailed_tests.cpp
DEBUG_TEST_SRCS = debug_test.cpp

# 헤더 파일들
INCLUDES = -I$(INCLUDE_DIR) -I$(CURRENT_DIR)

# 오브젝트 파일들
HTTP_OBJS = $(HTTP_SRCS:.cpp=.o)
TEST_OBJS = $(TEST_SRCS:.cpp=.o)
DETAILED_TEST_OBJS = $(DETAILED_TEST_SRCS:.cpp=.o)
DEBUG_TEST_OBJS = $(DEBUG_TEST_SRCS:.cpp=.o)

# 기본 타겟
all: $(NAME) $(DETAILED_TEST) $(DEBUG_TEST)

# 메인 테스트 실행파일
$(NAME): $(HTTP_OBJS) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# 상세 테스트 실행파일
$(DETAILED_TEST): $(HTTP_OBJS) $(DETAILED_TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(DEBUG_TEST): $(HTTP_OBJS) $(DEBUG_TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# 오브젝트 파일 컴파일
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 테스트 실행
test: $(NAME)
	@echo "=== 기본 테스트 실행 ==="
	./$(NAME)

detailed: $(DETAILED_TEST)
	@echo "=== 상세 테스트 실행 ==="
	./$(DETAILED_TEST)

# 메모리 체크
valgrind: $(NAME)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(NAME)

valgrind-detailed: $(DETAILED_TEST)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(DETAILED_TEST)

# 모든 테스트 실행
test-all: test detailed debug

# 클린업
clean:
	rm -f $(HTTP_OBJS) $(TEST_OBJS) $(DETAILED_TEST_OBJS) $(DEBUG_TEST_OBJS)

fclean: clean
	rm -f $(NAME) $(DETAILED_TEST) $(DEBUG_TEST)

re: fclean all

# 개발 도구들
debug: $(DEBUG_TEST)
	@echo "=== 디버그 테스트 실행 ==="
	./$(DEBUG_TEST)

# 컴파일 체크 (경고를 에러로 처리하지 않음)
check: CXXFLAGS := $(filter-out -Werror,$(CXXFLAGS))
check: all

# 도움말
help:
	@echo "사용 가능한 타겟들:"
	@echo "  all          - 모든 테스트 프로그램 컴파일"
	@echo "  test         - 기본 테스트 실행"
	@echo "  detailed     - 상세 테스트 실행"
	@echo "  test-all     - 모든 테스트 실행"
	@echo "  valgrind     - 메모리 체크 (기본 테스트)"
	@echo "  valgrind-detailed - 메모리 체크 (상세 테스트)"
	@echo "  debug        - 디버그 모드로 컴파일"
	@echo "  check        - 경고를 에러로 처리하지 않고 컴파일"
	@echo "  clean        - 오브젝트 파일 삭제"
	@echo "  fclean       - 모든 생성 파일 삭제"
	@echo "  re           - 전체 재컴파일"
	@echo "  help         - 이 도움말 출력"

# 의존성 확인
deps:
	@echo "=== 의존성 체크 ==="
	@echo "HttpRequest.hpp 위치: $(INCLUDE_DIR)/HttpRequest.hpp"
	@echo "HttpResponse.hpp 위치: $(INCLUDE_DIR)/HttpResponse.hpp"
	@echo "HttpRequest.cpp 위치: $(SRC_DIR)/HttpRequest.cpp"
	@echo "HttpResponse.cpp 위치: $(SRC_DIR)/HttpResponse.cpp"
	@ls -la $(INCLUDE_DIR)/Http*.hpp 2>/dev/null || echo "헤더 파일을 찾을 수 없습니다"
	@ls -la $(SRC_DIR)/Http*.cpp 2>/dev/null || echo "소스 파일을 찾을 수 없습니다"

.PHONY: all test detailed test-all valgrind valgrind-detailed clean fclean re debug check help deps