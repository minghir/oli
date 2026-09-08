include ../Makefile.plugin.common

ifdef IS_WINDOWS
    LDFLAGS += oli_db.def -lgdi32 -lcomctl32 -luser32 -lodbc32 -Wl,--kill-at -Wl,--enable-stdcall-fixup
    # Folosește += pentru a ADAUGA fișierele, nu pentru a le înlocui pe cele locale
    SRCS += ../../StringUtils.cpp ../../ConsoleManager.cpp ../../OliEngine.cpp ../../OliCompiler.cpp ../../PortTools.cpp ../../ASTNode.cpp ../../OliFuncHandlers.cpp ../../OliExecuteBytecode.cpp
else
    LDFLAGS += -lodbc
endif