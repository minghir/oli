CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

# Detectarea Sistemului de Operare
ifeq ($(OS),Windows_NT)
    # Setări pentru Windows (MSYS2/MinGW)
    TARGET = oli_plugin.dll
    LDFLAGS = -shared
    CLEAN_CMD = rm -f *.o $(TARGET) *.a
else
    # Setări pentru Linux
    TARGET = oli_plugin.so
    LDFLAGS = -shared
    CXXFLAGS += -fPIC
    CLEAN_CMD = rm -f *.o $(TARGET)
endif

SRC = $(wildcard *.cpp)
OBJ = $(SRC:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(CLEAN_CMD)

.PHONY: all clean