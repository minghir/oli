# src/plugins/oli_winui/Makefile

# 1. Căile specifice acestui plugin (folosind / în loc de \)
LIB_REPORT_DIR     = D:/Programming/Aplicatii_C2019/ANC/vreport/libvreport
INCLUDE_REPORT_DIR = D:/Programming/Aplicatii_C2019/ANC/vreport/libvreport/src

# 2. Injectăm calea de include în CXXFLAGS înainte ca fișierul common să fie procesat
CXXFLAGS = -I$(INCLUDE_REPORT_DIR)

# 3. Definim unde caută linkerul bibliotecile (-L)
LDFLAGS_LOCAL = -L$(LIB_REPORT_DIR)

# 4. Definim bibliotecile specifice
ifeq ($(OS),Windows_NT)
    LIBS = $(LIB_REPORT_DIR)/lib_winui.a -lgdi32 -luser32 -lcomctl32 -lshcore -lopengl32 -lglu32
else
    LIBS = -lX11
endif

# 5. ABIA ACUM includem logica globală (care va înghiți setările de mai sus)
include ../Makefile.plugin.common