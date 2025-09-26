# FileUtils Test Makefile

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g
TARGET = fileutils_test

# Source files
SRCS = FileUtilsTest.cpp FileUtils.cpp

# Object files
OBJS = FileUtilsTest.o FileUtils.o

# Include paths
INCLUDES = -I.

# Default target
all: $(TARGET)

# Build the test executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Run tests
test: $(TARGET)
	@echo "=== Running FileUtils Tests ==="
	./$(TARGET)

# Memory check with valgrind
valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET)

# Clean object files
clean:
	rm -f $(OBJS)

# Clean all generated files
fclean: clean
	rm -f $(TARGET)
	rm -rf test_files

# Rebuild
re: fclean all

# Debug build
debug: CXXFLAGS += -DDEBUG
debug: re

# Help
help:
	@echo "Available targets:"
	@echo "  all          - Build test executable"
	@echo "  test         - Build and run tests"
	@echo "  valgrind     - Run tests with memory checking"
	@echo "  clean        - Remove object files"
	@echo "  fclean       - Remove all generated files"
	@echo "  re           - Rebuild from scratch"
	@echo "  debug        - Debug build"
	@echo "  help         - Show this help"

.PHONY: all test valgrind clean fclean re debug help