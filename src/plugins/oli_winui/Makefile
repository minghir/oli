ifeq ($(OS),Windows_NT)
    LIBS += -lgdi32 -luser32 -lcomctl32 -lshcore -lopengl32 -lglu32 -luxtheme
else
    LIBS += -lX11
endif

# 5. Includem logica comună
include ../Makefile.plugin.common
