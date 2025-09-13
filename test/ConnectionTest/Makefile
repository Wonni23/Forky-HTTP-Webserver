# 컴파일러 설정
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

# 타겟 이름
TARGET = test

# 소스 파일들
SRCS = ClientTest.cpp ConfCascader.cpp ConfParser.cpp ConfApplicator.cpp main.cpp EventLoop.cpp Server.cpp

# 오브젝트 파일들
OBJS = ClientTest.o ConfCascader.o ConfParser.o ConfApplicator.o main.o EventLoop.o Server.o

# Include 경로 (현재 디렉토리의 헤더 파일들 포함)
INCLUDES = -I.

# 기본 타겟
all: $(TARGET)

# 실행 파일 생성 규칙
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

# .cpp 파일을 .o 파일로 컴파일하는 규칙
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 오브젝트 파일 정리
clean:
	rm -f $(OBJS)

# 모든 생성 파일 정리
fclean: clean
	rm -f $(TARGET)

# 전체 재빌드
re: fclean all

# 의존성 규칙 (헤더 파일 변경 시 재컴파일)
ClientTest.o: ClientTest.cpp ClientTest.hpp Common.hpp
ConfCascader.o: ConfCascader.cpp ConfCascader.hpp ConfigDTO.hpp
ConfParser.o: ConfParser.cpp ConfParser.hpp ConfigDTO.hpp
ConfApplicator.o: ConfApplicator.cpp ConfApplicator.hpp ConfigDTO.hpp Server.hpp  # ✅ 추가
main.o: main.cpp webserv.hpp Server.hpp ConfParser.hpp ConfCascader.hpp ConfApplicator.hpp  # ✅ 의존성 추가
EventLoop.o: EventLoop.cpp EventLoop.hpp Server.hpp
Server.o: Server.cpp Server.hpp Common.hpp ClientTest.hpp  # ✅ ClientTest.hpp 의존성 추가

.PHONY: all clean fclean re

