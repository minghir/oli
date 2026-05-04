CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -Iinclude -MMD -MP
# -MMD -MP generează automat dependențele (.d) pentru headere

# Ordinea LDFLAGS este critică: bibliotecile se pun DUPĂ obiecte
LDFLAGS = -lreadline

SRC_DIR = src
OLIC_DIR = src/olic
BUILD_DIR = build
TARGET = oli

# Identificăm toate fișierele sursă
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
# Transformăm căile sursă în căi pentru obiecte în build/
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
# Fișierele de dependență
DEPS = $(OBJS:.o=.d)


# Definim unde sunt pluginurile
PLUGIN_ROOT = src/plugins
# Găsește toate directoarele care conțin un Makefile în src/plugins/
PLUGINS = $(wildcard $(PLUGIN_ROOT)/*/. )

# Regula implicită
all: plugins olic $(TARGET)

# Compilează executabilul
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

# Regula de compilare a obiectelor
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Include fișierele .d generate (dacă există)
-include $(DEPS)


# Regula care compilează toate pluginurile gasite
plugins:
	@for dir in $(PLUGINS); do \
		echo "Building plugin in $$dir..."; \
		$(MAKE) -C $$dir; \
	done
# Regula pentru plugin
# Folosim @ ca să nu aglomerăm consola dacă nu e cazul

olic:
	@echo "Building olic..."
	$(MAKE) -C $(OLIC_DIR)

clean:
	@echo "Cleaning up..."
	rm -rf $(BUILD_DIR) $(TARGET)
	@for dir in $(PLUGINS); do \
		$(MAKE) -C $$dir clean; \
	done
	$(MAKE) -C $(OLIC_DIR) clean
	
.PHONY: all clean plugins $(TARGET) olic
