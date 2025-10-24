# CGI Response Test Makefile

NAME = cgi_response_test

# Compiler and flags
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g

# Directories
SRC_DIR = ../../src
INCLUDE_DIR = ../../include

# Source files (all except main.cpp)
SRCS = $(shell find $(SRC_DIR) -name "*.cpp" ! -name "main.cpp")
TEST_SRCS = cgi_response_test.cpp

# Header includes
CPPFLAGS = -I$(INCLUDE_DIR)

# Default target
all: $(NAME)

# Build executable (compile all at once like main Makefile)
$(NAME): $(SRCS) $(TEST_SRCS)
	@echo "🔨 Compiling and linking CgiResponse test..."
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(SRCS) $(TEST_SRCS) -o $(NAME)
	@echo "✓ $(NAME) built successfully"

# Run tests
test: $(NAME)
	@echo "=== Running CgiResponse Tests ==="
	./$(NAME)

# Memory check
valgrind: $(NAME)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(NAME)

# Clean
clean:
	rm -f $(ALL_OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

# Help
help:
	@echo "Available targets:"
	@echo "  all          - Build test executable"
	@echo "  test         - Run tests"
	@echo "  valgrind     - Run tests with memory check"
	@echo "  clean        - Remove object files"
	@echo "  fclean       - Remove all generated files"
	@echo "  re           - Rebuild everything"
	@echo "  help         - Show this help message"

.PHONY: all test valgrind clean fclean re help
