CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -I.

TARGET = olic

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)


COMMON_OBJS = $(wildcard ../../build/StringUtils.o ../../build/ConsoleManager.o ../../build/PortTools.o)

all: $(TARGET)

$(TARGET): $(OBJS) $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(COMMON_OBJS)
	cp $(TARGET) ../../
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
