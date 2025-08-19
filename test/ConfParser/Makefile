CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
FILE_TEST = file_test
STRING_TEST = string_test

SRCDIR = .
OBJDIR = obj

# 소스 파일들
FILE_TEST_SOURCES = ConfParser.cpp file_test.cpp
STRING_TEST_SOURCES = ConfParser.cpp confParserExampleUsage.cpp

# 오브젝트 파일들
FILE_TEST_OBJECTS = $(FILE_TEST_SOURCES:%.cpp=$(OBJDIR)/%.o)
STRING_TEST_OBJECTS = $(STRING_TEST_SOURCES:%.cpp=$(OBJDIR)/%.o)

# 헤더 파일들
HEADERS = ConfParser.hpp ConfigDTO.hpp

# 기본 타겟을 file_test로 변경
all: $(FILE_TEST)

$(FILE_TEST): $(FILE_TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(STRING_TEST): $(STRING_TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# 헤더 의존성을 포함한 오브젝트 파일 생성 규칙
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(HEADERS)
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(FILE_TEST) $(STRING_TEST)

re: fclean all

# 별칭들
file: $(FILE_TEST)
	./$(FILE_TEST)

string: $(STRING_TEST)
	./$(STRING_TEST)

test_custom: $(FILE_TEST)
	./$(FILE_TEST) test.conf

.PHONY: all clean fclean re file string test_custom