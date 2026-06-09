# src/plugins/oli_canvas/Makefile
include ../Makefile.plugin.common

ifdef IS_WINDOWS
    LIBS = -lopengl32 -lgdi32 -lglu32 -luser32
else
    LIBS = -lX11
endif
