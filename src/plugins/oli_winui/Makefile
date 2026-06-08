ifeq ($(OS),Windows_NT)
	CXXFLAGS += -D_WIN32_WINNT=0x0A00 -I../../
    LIBS += -lgdi32 -luser32 -lcomctl32 -lshcore -lopengl32 -lglu32 -luxtheme  -lcomdlg32 

    SRCS = pugixml-1.15/src/pugixml.cpp ../../ConsoleManager.cpp 

    # 5. Includem logica comună
    include ../Makefile.plugin.common
else
all:
	@echo "Skipping oli_winui plugin on non-Windows platforms."

clean:
	@echo "Nothing to clean for oli_winui on non-Windows."
	@:
endif
