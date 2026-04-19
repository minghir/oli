CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -fPIC
LDFLAGS = -shared

SRC = $(wildcard *.cpp)
OBJ = $(SRC:.cpp=.o)

TARGET = oli_plugin.so

all: $(TARGET)

$(TARGET): $(OBJ)
    $(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
    $(CXX) $(CXXFLAGS) -c $< -o $@

clean:
    rm -f *.o $(TARGET)

.PHONY: all clean