# src/plugins/oli_sprites/Makefile

# 1. Include fișierul comun mai întâi
include ../Makefile.plugin.common

# 2. Folosește variabila IS_WINDOWS definită în fișierul comun
ifdef IS_WINDOWS
    LIBS = -lgdiplus
else
    LIBS = -lX11
endif