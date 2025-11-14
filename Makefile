# ============================================================================== #
#                       webserv test Makefile (Debug Version)                         #
# ============================================================================== #

# --- 변수 설정 (Variables) ---

# 컴파일러 및 플래그
CXX			:= c++
CXXFLAGS	:= -Wall -Wextra -Werror -std=c++98 -DDEBUG

# 프로젝트 이름
NAME		:= webserv

# 디렉토리 경로
SRC_DIR		:= src
INC_DIR		:= include
OBJ_DIR		:= obj

# -I 플래그 추가
CPPFLAGS	:= -I$(INC_DIR)

# --- 소스 파일 명시적 나열 ---
# 'find' 대신 모든 .cpp 파일을 직접 지정합니다.
SRCS		:= $(SRC_DIR)/main.cpp \
			   $(SRC_DIR)/cgi/CgiExecutor.cpp \
			   $(SRC_DIR)/cgi/CgiResponse.cpp \
			   $(SRC_DIR)/config/ConfApplicator.cpp \
			   $(SRC_DIR)/config/ConfCascader.cpp \
			   $(SRC_DIR)/config/ConfigManager.cpp \
			   $(SRC_DIR)/config/ConfParser.cpp \
			   $(SRC_DIR)/http/HttpController.cpp \
			   $(SRC_DIR)/http/HttpRequest.cpp \
			   $(SRC_DIR)/http/HttpResponse.cpp \
			   $(SRC_DIR)/http/MultipartFormDataParser.cpp \
			   $(SRC_DIR)/http/RequestRouter.cpp \
			   $(SRC_DIR)/http/StatusCode.cpp \
			   $(SRC_DIR)/http/handler/DeleteHandler.cpp \
			   $(SRC_DIR)/http/handler/GetHandler.cpp \
			   $(SRC_DIR)/http/handler/PostHandler.cpp \
			   $(SRC_DIR)/server/Client.cpp \
			   $(SRC_DIR)/server/EventLoop.cpp \
			   $(SRC_DIR)/server/Server.cpp \
			   $(SRC_DIR)/utils/FileManager.cpp \
			   $(SRC_DIR)/utils/FileUtils.cpp \
			   $(SRC_DIR)/utils/PathResolver.cpp \
			   $(SRC_DIR)/utils/StringUtils.cpp

# --- 오브젝트 파일 생성 ---
# 소스 파일 목록(SRCS)을 기반으로 오브젝트 파일(.o) 목록을 생성합니다.
# 예: src/main.cpp -> obj/main.o
# 예: src/cgi/CgiExecutor.cpp -> obj/cgi/CgiExecutor.o
OBJS		:= $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)


# --- 규칙 설정 (Rules) ---

# 기본 규칙: 'make' 또는 'make all' 실행 시 $(NAME)을 빌드
all: $(NAME)

# 실행 파일 생성 규칙 (링킹)
# .o 파일들을 의존성으로 받아 링킹하여 최종 실행 파일을 생성합니다.
$(NAME): $(OBJS)
	@echo "🔗 Linking object files into $(NAME)..."
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "✅ webserv build complete!"

# 오브젝트 파일 생성 규칙 (컴파일)
# .cpp 파일 하나를 컴파일하여 .o 파일을 생성하는 패턴 규칙입니다.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "🔨 Compiling $<..."
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# 디버그 심볼 포함 (-g) 및 DEEP 플래그 추가
deep: CXXFLAGS += -g -DDEEP
deep: all

# 릴리즈 타겟 (모든 로그 비활성화 및 최적화)
release: CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -O3
release: all

# 정리 규칙 (오브젝트 파일)
clean:
	@echo "🧹 Cleaning object files..."
	@rm -rf $(OBJ_DIR)

# 전체 정리 규칙 (오브젝트 파일 + 실행 파일)
fclean: clean
	@echo "🗑️  Cleaning executable..."
	@rm -f $(NAME)

# 재빌드 규칙
re: fclean all

# 가상 타겟 선언
.PHONY: all clean fclean re deep release