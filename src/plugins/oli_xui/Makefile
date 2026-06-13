# 1. Includem fișierul comun pentru plugin-uri
include ../Makefile.plugin.common

# Detecție automată sau manuală pentru GTK. Folosim GTK+-3.0 ca standard industrial.
GTK_FLAGS := $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS  := $(shell pkg-config --libs gtk+-3.0)

# Injectăm flag-urile GTK în procesul global de compilare
CXXFLAGS += $(GTK_FLAGS) -I../../ -Ipugixml-1.15/src/
LIBS += $(GTK_LIBS)

ifdef IS_WINDOWS
    # Dacă îl compilăm pe Windows (folosind MSYS2/MinGW cu GTK instalat), 
    # avem nevoie și de câteva librării native Win32 pentru managementul OS
    CXXFLAGS += -D_WIN32_WINNT=0x0A00
    LIBS += -lshcore -lcomdlg32
endif

# =================================================================
# REGULI DE COMPILARE COMPONENTE EXTERNE
# =================================================================

# 1. Compilăm separat parserul XML pentru configurări (Sintaxe/Teme)
#pugixml.o: pugixml-1.15/src/pugixml.cpp
#	$(CXX) $(CXXFLAGS) -c $< -o $@

# 2. Compilăm trackerul de loguri (ConsoleManager) shared cu engine-ul gazdă
ConsoleManager.o: ../../ConsoleManager.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

StringUtils.o: ../../StringUtils.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

PortTools.o: ../../PortTools.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# =================================================================
# RECOLOAREA ȘI LINK-AREA FINALĂ A PLUGINULUI (oli_xui.so / oli_xui.dll)
# =================================================================

# $(OBJS) va conține automat xApp.o, xWindow.o, xContainer.o, xControl.o, EventDispatcher.o etc.
$(TARGET): $(OBJS) ConsoleManager.o StringUtils.o PortTools.o
	$(CXX) -o $@ $(OBJS) ConsoleManager.o StringUtils.o PortTools.o $(LDFLAGS) $(LIBS)
	@mkdir -p ../../../plugins
	cp $(TARGET) ../../../plugins

# Suprascriem regula 'all' implicită pentru a ne asigura că totul rulează corect
all: $(TARGET)

#clean:
#	rm -f *.o $(TARGET)