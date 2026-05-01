CXX = g++
CXXFLAGS = -std=c++20 -Wall -fPIC -O2 -I../../../include
LDFLAGS = -shared

TARGET = ../../../x64/Release/oli_console.so

SRC = oli_console.cpp
OBJ = oli_console.o

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p ../../../x64/Release
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)