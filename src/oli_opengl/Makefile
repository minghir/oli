# src/plugins/oli_canvas/Makefile
ifeq ($(OS),Windows_NT)
LIBS = -lopengl32 -lgdi32 -luser32
else
LIBS = -lX11
endif
# src/plugins/oli_sprites/Makefile
include ../Makefile.plugin.common