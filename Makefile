# ============================================================================== #
#                       webserv test Makefile (Debug Version)                         #
# ============================================================================== #

# --- 변수 설정 (Variables) ---

# 컴파일러 및 플래그
CXX         := c++
CXXFLAGS    := -Wall -Wextra -Werror -std=c++98 -DDEBUG

# 프로젝트 이름
NAME        := webserv

# 디렉토리 경로
SRC_DIR     := src
INC_DIR     := include

# -I 플래그 추가
CPPFLAGS    := -I$(INC_DIR)

# 소스 파일 자동 탐색
# find 명령어로 src 디렉토리와 그 하위의 모든 .cpp 파일을 찾음
SRCS        := $(shell find $(SRC_DIR) -name "*.cpp")


# --- 규칙 설정 (Rules) ---

# 기본 규칙: 'make' 또는 'make all' 실행 시 $(NAME)을 빌드
all: $(NAME)

# 실행 파일 생성 규칙
# [수정] .o 파일 의존성을 제거하고, 모든 .cpp 파일을 직접 컴파일 및 링크
$(NAME): $(SRCS)
	@echo "🔨 Compiling and linking all sources into $(NAME)..."
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(SRCS) -o $(NAME)
	@echo "✅ webserv build complete!"

deep: CXXFLAGS += -g -DDEEP
deep: all

# 릴리즈 타겟 (모든 로그 비활성화)
release: CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -O3
release: all

# 정리 규칙
clean:
	# [수정] OBJ_DIR이 없으므로, 해당 디렉토리를 삭제하는 명령 제거
	@echo "🧹 Nothing to clean, no object files were generated."

fclean: clean
	@echo "🗑️  Cleaning executable..."
	@rm -f $(NAME)

re: fclean all

# 가상 타겟 선언
.PHONY: all clean fclean re


# # ============================================================================== #
# #                                 webserv Makefile                               #
# # =================================D============================================= #

# # --- 변수 설정 (Variables) ---

# # 컴파일러 및 플래그
# CXX			:= c++
# CXXFLAGS	:= -Wall -Wextra -Werror -std=c++98 -g

# # 프로젝트 이름
# NAME		:= webserv

# # 디렉토리 경로
# SRC_DIR		:= src
# OBJ_DIR		:= obj
# INC_DIR		:= include

# # -I 플래그 추가
# CPPFLAGS	:= -I$(INC_DIR)

# # 소스 파일 자동 탐색
# # find 명령어로 src 디렉토리와 그 하위의 모든 .cpp 파일을 찾음
# SRCS		:= $(shell find $(SRC_DIR) -name "*.cpp")

# # 오브젝트 파일 경로 생성
# # src/main.cpp -> obj/main.o 와 같이 변환
# OBJS		:= $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))


# # --- 규칙 설정 (Rules) ---

# # 기본 규칙: 'make' 또는 'make all' 실행 시 $(NAME)을 빌드
# all: $(NAME)

# # 실행 파일 생성 규칙
# $(NAME): $(OBJS)
# 	@echo "🔗 Linking objects to create $(NAME)..."
# 	@$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
# 	@echo "✅ webserv build complete!"

# # 오브젝트 파일 생성 규칙
# # 이 규칙은 각 .cpp 파일에 대해 .o 파일을 생성함
# $(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
# 	@mkdir -p $(dir $@)  # obj 폴더 및 하위 폴더가 없으면 생성
# 	@echo "🔨 Compiling $<..."
# 	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# # 정리 규칙
# clean:
# 	@echo "🧹 Cleaning object files..."
# 	@rm -rf $(OBJ_DIR)

# fclean: clean
# 	@echo "🗑️  Cleaning executable..."
# 	@rm -f $(NAME)

# re: fclean all

# # 가상 타겟 선언
# .PHONY: all clean fclean re

