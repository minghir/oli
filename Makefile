CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

# Definirea librăriilor
LDFLAGS = -lreadline

SRC_DIR = src
BUILD_DIR = build
TARGET = oli

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Compilează plugin-ul
plugin:
    $(MAKE) -C oli_plugin

# Compilează executabilul + plugin-ul
all: $(TARGET) plugin

$(TARGET): $(OBJS)
    $(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
    @mkdir -p $(BUILD_DIR)
    $(CXX) $(CXXFLAGS) -c $< -o $@

clean:
    rm -rf $(BUILD_DIR) $(TARGET)
    $(MAKE) -C oli_plugin clean

.PHONY: all clean plugin
