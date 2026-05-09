#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "OliCompiler.hpp"
#include "OliBytecode.hpp"
#include "../StringUtils.hpp"
#include "../vDataSerialize.hpp"
#include "../ConsoleManager.hpp"


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
    if (!ofs.is_open()) {
        LOG_ERROR(L"Nu am putut deschide fisierul pentru scriere: " + str_to_wstr(path));
        return;
    }

    // Lăsăm namespace-ul tău să gestioneze totul uniform
    vDataSerialize::serializeChunk(chunk, ofs);

    ofs.close();
}


int main(int argc, char* argv[]) {
    ConsoleManager::getInstance().initialize();
    ConsoleManager::getInstance().setMinLogLevel(LogLevel::DEBUG);
    if (argc < 2) {
        LOG_RAW(L"Oli Compiler (olic) v1.0");
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = (argc > 2) ? argv[2] : inputPath + "c";

    try {
        LOG_DEBUG(L"1. Citire fisier...");
        std::wstring source = readFile(inputPath);

        LOG_DEBUG(L"2. Pornire Compilator...");
        OliCompiler compiler;
        OliChunk chunk = compiler.compile(source);

        LOG_DEBUG(L"3. Compilare terminata. Incercare salvare bytecode...");
        saveBytecode(chunk, outputPath);
        LOG_DEBUG(L"   [OK] Bytecode salvat in: " + str_to_wstr(outputPath));

        LOG_DEBUG(L"4. Generare cale Assembly...");
        // Fix pentru crash-ul substr: verificam daca exista punctul
        std::wstring assemblyPath = str_to_wstr(outputPath);
        size_t dotPos = assemblyPath.find_last_of(L'.');
        if (dotPos != std::wstring::npos) {
            assemblyPath = assemblyPath.substr(0, dotPos) + L".olia";
        }
        else {
            assemblyPath += L".olia";
        }

        LOG_DEBUG(L"5. Incepere Dezasamblare (Suspectul principal)...");
        auto& logger = ConsoleManager::getInstance();
        if (logger.enableFileLogging(assemblyPath, false)) {
            logger.writeRaw(L"--- OLI ASSEMBLY LISTING ---\n");

            // AICI e testul suprem: daca programul dispare acum, disassembleChunk e devina
            std::wstring disassembly = disassembleChunk(chunk);
            logger.writeRaw(disassembly);

            logger.closeLogFile();
            LOG_DEBUG(L"   [OK] Assembly generat: " + assemblyPath);
        }

        LOG_RAW(L"\n>>> SUCCESS! Finalizat cu bine. <<<");
        LOG_DEBUG(L"Dimensiune totala: " + std::to_wstring(chunk.code.size()) + L" bytes.");
    }
    catch (const std::exception& e) {
        LOG_ERROR(L"\n!!! CRASH DETECTAT: " + str_to_wstr(e.what()));
        return 1;
    }
    catch (...) {
        LOG_ERROR(L"\n!!! CRASH NECUNOSCUT (Probabil Segmentation Fault in disassembleChunk)");
        return 1;
    }

    return 0;
}