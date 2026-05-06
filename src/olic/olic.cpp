#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "OliCompiler.hpp"
#include "OliBytecode.hpp"
#include "../StringUtils.hpp"
#include "../vDataSerialize.hpp"

// Funcție helper pentru a citi tot fișierul în memorie
std::wstring readFile(const std::string& path) {
    std::wifstream wif(path);
    wif.imbue(std::locale("")); // Suport pentru caractere wide/unicode
    std::wstringstream wss;
    wss << wif.rdbuf();
    return wss.str();
}

// Funcție rudimentară pentru a salva chunk-ul într-un fișier binar
void saveBytecode(const OliChunk& chunk, const std::string& path) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs.is_open()) {
        std::cerr << "Failed to open output file: " << path << std::endl;
        return;
    }

    // 1. Salvăm numărul de constante (Tabelul de simboluri/valori)
    uint32_t constCount = (uint32_t)chunk.constants.size();
    ofs.write(reinterpret_cast<const char*>(&constCount), sizeof(constCount));

    // --- AICI SERIALIZEZI FIECARE CONSTANTĂ ---
    for (const auto& constant : chunk.constants) {
        vDataSerialize::serializevData(constant, ofs); // Folosește funcția din vDataSerialize.hpp
    }

    // 2. Salvăm dimensiunea codului binar
    uint32_t codeSize = (uint32_t)chunk.code.size();
    ofs.write(reinterpret_cast<const char*>(&codeSize), sizeof(codeSize));

    // 3. Salvăm instrucțiunile (Bytecode-ul)
    ofs.write(reinterpret_cast<const char*>(chunk.code.data()), chunk.code.size());

    ofs.close();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Oli Compiler (olic) v1.0" << std::endl;
        std::cout << "Usage: olic <file.oli> [output.olic]" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = (argc > 2) ? argv[2] : inputPath + "c"; // Default: file.olic
	
	try {
        std::locale::global(std::locale("C"));
    } catch (...) {
        std::wcout<<"Eroare LOCALE"<<std::endl;	
    }
	
    try {
        std::wcout << L"Compiling: " << std::wstring(inputPath.begin(), inputPath.end()) << L"..." << std::endl;

        std::wstring source = readFile(inputPath);

        OliCompiler compiler;
        OliChunk chunk = compiler.compile(source);

        saveBytecode(chunk, outputPath);
        // 2. Salvează assembly-ul .olia prin ConsoleManager
        std::wstring assemblyPath = str_to_wstr(outputPath);
        assemblyPath = assemblyPath.substr(0, assemblyPath.find_last_of(L'.')) + L".olia";

        auto& logger = ConsoleManager::getInstance();
        if (logger.enableFileLogging(assemblyPath, false)) {
            logger.writeRaw(L"--- OLI ASSEMBLY LISTING ---\n");
            logger.writeRaw(disassembleChunk(chunk));
            logger.closeLogFile();

            std::wcout << L"Alpha file generated: " << assemblyPath << std::endl;
        }

        std::cout << "Success! Generated: " << outputPath << " (" << chunk.code.size() << " bytes)" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Compiler Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}