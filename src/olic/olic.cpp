#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "OliCompiler.hpp"
#include "OliBytecode.hpp"
#include "../StringUtils.hpp"
#include "../vDataSerialize.hpp"

// Funcție helper pentru a citi tot fișierul în memorie
/*
std::wstring readFile(const std::string& path) {
    std::wifstream wif(path);
    wif.imbue(std::locale("")); // Suport pentru caractere wide/unicode
    std::wstringstream wss;
    wss << wif.rdbuf();
    return wss.str();
}
*/

std::wstring readFile(const std::string& path) {
    std::wifstream wif(path);

    // Locale universal compatibil în MSYS2, Linux și Windows
    try {
        wif.imbue(std::locale("C.UTF-8"));
    } catch (...) {
        // Fallback dacă MSYS2 nu are locale setat
        wif.imbue(std::locale::classic());
    }

    std::wstringstream wss;
    wss << wif.rdbuf();
    return wss.str();
}


void saveBytecode(const OliChunk& chunk, const std::string& path) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs.is_open()) return;

    // 1. Constante
    uint32_t constCount = (uint32_t)chunk.constants.size();
    ofs.write((char*)&constCount, 4);
    for (const auto& c : chunk.constants) {
        // FIX: Inversat (Date, Stream)
        vDataSerialize::serializevData(c, ofs);
    }

    // 2. Cod
    uint32_t codeSize = (uint32_t)chunk.code.size();
    ofs.write((char*)&codeSize, 4);
    ofs.write((char*)chunk.code.data(), codeSize);

    // 3. Proceduri
    uint32_t procCount = (uint32_t)chunk.procedures.size();
    ofs.write((char*)&procCount, 4);
    for (auto const& [name, proc] : chunk.procedures) {
        // FIX: Inversat (Date, Stream)
        vDataSerialize::serializeWString(proc.name, ofs);

        uint32_t pCount = (uint32_t)proc.params.size();
        ofs.write((char*)&pCount, 4);
        for (const auto& p : proc.params) {
            vDataSerialize::serializeWString(p, ofs); // FIX: Inversat
        }

        uint8_t variadic = proc.isVariadic ? 1 : 0;
        ofs.write((char*)&variadic, 1);

        // RECURSIVITATE
        vDataSerialize::serializeChunk(*proc.compiledBody, ofs);
    }
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
