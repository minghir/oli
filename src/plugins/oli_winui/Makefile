ifeq ($(OS),Windows_NT)
	CXXFLAGS += -D_WIN32_WINNT=0x0A00 -I../../
    LIBS += -lgdi32 -luser32 -lcomctl32 -lshcore -lopengl32 -lglu32 -luxtheme  -lcomdlg32
else
    LIBS += -lX11
endif

SRCS = pugixml-1.15/src/pugixml.cpp ../../ConsoleManager.cpp 

# 5. Includem logica comună
include ../Makefile.plugin.common
