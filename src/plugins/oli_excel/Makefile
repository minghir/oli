# 1. Adăugăm sursele comune ale motorului ÎNAINTE de include
SRCS += ../../StringUtils.cpp ../../ConsoleManager.cpp ../../OliEngine.cpp \
        ../../OliCompiler.cpp ../../PortTools.cpp ../../ASTNode.cpp \
        ../../OliFuncHandlers.cpp ../../OliExecuteBytecode.cpp

# 2. Includem fișierul comun
include ../Makefile.plugin.common

# Flag-uri comune de include pentru OpenXLSX, miniz și pugixml (valabile pe ambele platforme)
CXXFLAGS += -std=c++20 -D_CRT_SECURE_NO_WARNINGS -Ipugixml-1.15/src -IOpenXLSX -IOpenXLSX/headers -IOpenXLSX/sources -IOpenXLSX/headers/detail -Iminiz

ifdef IS_WINDOWS
    # Biblioteci specifice Windows
    LIBS += -lgdi32 -luser32 -lcomctl32
else
    # Flag-uri specifice Linux (dacă e nevoie, de ex. -pthread)
    LIBS += -pthread
endif

# 3. Compilare automată pentru pugixml (Cross-platform)
pugixml.o: pugixml-1.15/src/pugixml.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 4. Compilare pentru toate sursele .cpp din OpenXLSX/sources (Cross-platform)
%.o: OpenXLSX/sources/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 5. Compilare pentru sursele .c din miniz (Cross-platform)
%.o: miniz/%.c
	$(CC) $(CXXFLAGS) -c $< -o $@

# 6. Lista fișierelor obiect OpenXLSX, miniz și pugixml
OPENXLSX_OBJS = XLCell.o XLCellIterator.o XLCellRange.o XLCellReference.o \
                XLCellValue.o XLColor.o XLColumn.o XLComments.o XLContentTypes.o \
                XLDateTime.o XLDocument.o XLDrawing.o XLFormula.o XLMergeCells.o \
                XLProperties.o XLRelationships.o XLRow.o XLRowData.o XLSharedStrings.o \
                XLSheet.o XLStyles.o XLTables.o XLWorkbook.o XLXmlData.o XLXmlFile.o \
                XLXmlParser.o XLZipArchive.o miniz.o miniz_tdef.o miniz_tinfl.o \
                miniz_zip.o pugixml.o

# 7. Regula de link-are finală (Cross-platform)
$(TARGET): $(OBJS) $(OPENXLSX_OBJS)
	$(CXX) -shared -o $@ $(OBJS) $(OPENXLSX_OBJS) $(LDFLAGS) $(LIBS)
	mkdir -p ../../../plugins
	cp $(TARGET) ../../../plugins