include ../Makefile.plugin.common

ifdef IS_WINDOWS
 LDFLAGS += -lgdi32 -lcomctl32 -luser32 -lodbc32
	SRCS = ../../StringUtils.cpp ../../ConsoleManager.cpp ../../OliEngine.cpp ../../OliCompiler.cpp ../../PortTools.cpp ../../ASTNode.cpp ../../OliFuncHandlers.cpp ../../OliExecuteBytecode.cpp
else
    LDFLAGS += LDFLAGS += -lodbc
endif