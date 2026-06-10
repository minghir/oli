# src/plugins/oli_canvas/Makefile

# 1. Include mai întâi fișierul comun pentru ca variabilele să fie setate
include ../Makefile.plugin.common

# 2. Acum poți folosi IS_WINDOWS pentru a seta LIBS
ifdef IS_WINDOWS
    LIBS = -lgdi32 -lcomctl32 -luser32
else
    LIBS = -lX11
endif

# 3. Restul regulilor sunt deja moștenite prin include