CXX = g++
CXXFLAGS = -std=c++20 -DUNICODE -D_UNICODE -Wall -Wextra -O3 -Iinclude -MMD -MP
# -MMD -MP generează automat dependențele (.d) pentru headere

# Ordinea LDFLAGS este critică: bibliotecile se pun DUPĂ obiecte
LDFLAGS = -lreadline -static-libgcc -static-libstdc++

SRC_DIR = src
#OLIC_DIR = src/olic
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
all: plugins $(TARGET) 

# Compilează executabilul
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

# Regula de compilare a obiectelor
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Include fișierele .d generate (dacă există)
-include $(DEPS)


# Detectăm sistemul de operare
OS := $(shell uname -s)

# Regula care compilează toate pluginurile gasite
plugins:
	@for dir in $(PLUGINS); do \
		name=$$(basename $$(dirname $$dir)); \
		if [ "$$name" = "oli_opengl" -o "$$name" = "oli_winui" ]; then \
			if [ "$(OS)" = "Linux" ]; then \
				echo "Skipping plugin $$name on Linux"; \
				continue; \
			fi; \
		fi; \
		echo "Building plugin in $$dir..."; \
		$(MAKE) -C $$dir; \
	done
# Regula pentru plugin
# Folosim @ ca să nu aglomerăm consola dacă nu e cazul

#olic:
#	@echo "Building olic..."
#	$(MAKE) -C $(OLIC_DIR)

clean:
	@echo "Cleaning up..."
	# Șterge build-ul și executabilul principal
	rm -rf $(BUILD_DIR) $(TARGET)
	
	# Șterge plugin-urile compilate
	rm -rf plugins/*
	
	# Șterge fișierele reziduale din rădăcină
	rm -f *.dll *.so *.exe *.json *.ppm
	
	# --- ADAUGĂ ASTA PENTRU ȘTERGERE RECURSIVĂ ---
	# Caută și șterge toate fișierele .o și .d din orice subdirector
	find . -type f \( -name "*.o" -o -name "*.d" \) -delete
	
	# Execută clean și în plugin-uri
	@for dir in $(PLUGINS); do \
		$(MAKE) -C $$dir clean; \
	done
	@echo "Cleanup complete."
	
	
.PHONY: all clean plugins $(TARGET) 
