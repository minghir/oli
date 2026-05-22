# 1. Căile specifice pluginului
LIB_WINUI_DIR      = ../../../../lib_winui
INCLUDE_LIB_WINUI  = $(LIB_WINUI_DIR)/src

# 2. Adăugăm include-urile WinUI
CXXFLAGS += -I$(INCLUDE_LIB_WINUI)

# 3. Adăugăm calea către biblioteca WinUI
LDFLAGS += -L$(LIB_WINUI_DIR)

# 4. Bibliotecile necesare
ifeq ($(OS),Windows_NT)
    LIBS += -l_winui -lgdi32 -luser32 -lcomctl32 -lshcore -lopengl32 -lglu32
else
    LIBS += -lX11
endif

# 5. Includem logica comună
include ../Makefile.plugin.common
