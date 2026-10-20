# 1. Adăugăm sursele comune ale motorului ÎNAINTE de include
# Astfel, Makefile.plugin.common le va vedea și va genera automat fișierele .o
SRCS += ../../StringUtils.cpp ../../ConsoleManager.cpp ../../OliEngine.cpp \
        ../../OliCompiler.cpp ../../PortTools.cpp ../../ASTNode.cpp \
        ../../OliFuncHandlers.cpp ../../OliExecuteBytecode.cpp

# 2. Includem fișierul comun
include ../Makefile.plugin.common

ifdef IS_WINDOWS
    # Flag-uri și include-uri pentru OpenXLSX, miniz și pugixml
    CXXFLAGS += -std=c++20 -D_CRT_SECURE_NO_WARNINGS -Ipugixml-1.15/src -IOpenXLSX -IOpenXLSX/headers -IOpenXLSX/sources -IOpenXLSX/headers/detail -Iminiz
    LIBS += -lgdi32 -luser32 -lcomctl32

    # 3. Compilare automată pentru pugixml
    pugixml.o: pugixml-1.15/src/pugixml.cpp
		$(CXX) $(CXXFLAGS) -c $< -o $@

    # 4. Compilare pentru toate sursele .cpp din OpenXLSX/sources
    %.o: OpenXLSX/sources/%.cpp
		$(CXX) $(CXXFLAGS) -c $< -o $@

    # 5. Compilare pentru sursele .c din miniz
    %.o: miniz/%.c
		$(CC) $(CXXFLAGS) -c $< -o $@

    # 6. Lista EXACTĂ a fișierelor tale (generată din output-ul comenzii find)
    OPENXLSX_OBJS = XLCell.o XLCellIterator.o XLCellRange.o XLCellReference.o \
                    XLCellValue.o XLColor.o XLColumn.o XLComments.o XLContentTypes.o \
                    XLDateTime.o XLDocument.o XLDrawing.o XLFormula.o XLMergeCells.o \
                    XLProperties.o XLRelationships.o XLRow.o XLRowData.o XLSharedStrings.o \
                    XLSheet.o XLStyles.o XLTables.o XLWorkbook.o XLXmlData.o XLXmlFile.o \
                    XLXmlParser.o XLZipArchive.o miniz.o miniz_tdef.o miniz_tinfl.o \
                    miniz_zip.o pugixml.o

    # 7. Suprascriem regula de link-are finală
    $(TARGET): $(OBJS) $(OPENXLSX_OBJS)
		$(CXX) -o $@ $(OBJS) $(OPENXLSX_OBJS) $(LDFLAGS) $(LIBS)
		cp $(TARGET) ../../../plugins
else
    all:
		@echo "Skipping oli_excel on non-Windows..."
endif