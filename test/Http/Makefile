CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
SOURCES = http_test_main.cpp HttpRequest.cpp HttpResponse.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = http_test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS)

clean:
	rm -f $(OBJECTS)

fclean: clean
	rm -f $(TARGET)

re: fclean all

.PHONY: all clean fclean re