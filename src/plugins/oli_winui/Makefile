# 1. Includem fișierul comun
include ../Makefile.plugin.common

ifdef IS_WINDOWS
    CXXFLAGS += -D_WIN32_WINNT=0x0A00 -I../../ -Ipugixml-1.15/src/
    LIBS += -lgdi32 -luser32 -lcomctl32 -lshcore -lopengl32 -lglu32 -luxtheme -lcomdlg32
    
    # 1. Asigură-te că pugixml.o este compilat separat
    # Nu îl adăuga în SRCS (ca să nu fie tratat de regula generică din common)
    pugixml.o: pugixml-1.15/src/pugixml.cpp
	    $(CXX) $(CXXFLAGS) -c $< -o $@

    # 2. Recomandare: Scoate ConsoleManager.o din OBJS aici dacă există în oli.exe!
    # Pentru a rezolva "undefined reference", trebuie să te asiguri că fișierul
    # sursă al ConsoleManager.cpp este compilat undeva în proiect.
    # Dacă este compilat în oli.exe, asigură-te că este exportat (dllexport).
    # Dacă NU este, adaugă-l aici ca pe pugixml.o:
    ConsoleManager.o: ../../ConsoleManager.cpp
	    $(CXX) $(CXXFLAGS) -c $< -o $@
		
    
    # 3. Suprascrie regula de link-are pentru a include pugixml.o
    # $(OBJS) conține restul fișierelor .o din folder
    $(TARGET): $(OBJS) pugixml.o ConsoleManager.o
	    $(CXX) -o $@ $(OBJS) pugixml.o ConsoleManager.o $(LDFLAGS) $(LIBS)
	    cp $(TARGET) ../../../plugins
else
    all:
		@echo "Skipping oli_winui..."
endif