CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -Iinclude -MMD -MP
# -MMD -MP generează automat dependențele (.d) pentru headere

# Ordinea LDFLAGS este critică: bibliotecile se pun DUPĂ obiecte
LDFLAGS = -lreadline

SRC_DIR = src
BUILD_DIR = build
TARGET = oli

# Identificăm toate fișierele sursă
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
# Transformăm căile sursă în căi pentru obiecte în build/
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
# Fișierele de dependență
DEPS = $(OBJS:.o=.d)

# Regula implicită
all: plugin $(TARGET)

# Compilează executabilul
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

# Regula de compilare a obiectelor
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Include fișierele .d generate (dacă există)
-include $(DEPS)

# Regula pentru plugin
# Folosim @ ca să nu aglomerăm consola dacă nu e cazul
plugin:
	@echo "Building plugin..."
	$(MAKE) -C oli_plugin

clean:
	@echo "Cleaning up..."
	rm -rf $(BUILD_DIR) $(TARGET)
	$(MAKE) -C oli_plugin clean

.PHONY: all clean plugin
