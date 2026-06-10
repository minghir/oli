#include "Shell.hpp"
#include "OliEngine.hpp"
#include "App.hpp"
#include "ConsoleManager.hpp"
#include "vDataSerialize.hpp"
#include "OliCompiler.hpp"
#include "StringUtils.hpp" 
#include "PortTools.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <string>
#include <algorithm>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#include <sys/stat.h>
#endif



class oli : public App {
public:
    oli(RunMode rm) : App() { setRunMode(rm); };
    bool initConsole() override {
        vOliEngine shClient;
        Shell shell(shClient);
        shell.run();
        return true;
    }
};

int main(int argc, char* argv[]) {

    ConsoleManager::getInstance().initialize();
    ConsoleManager::getInstance().setMinLogLevel(LogLevel::LOG_ERROR);

    if (vOliEngine::runEmbeddedIfPresent(argv[0])) return 0;

    // --- SECTOR A: FLAG-URI (-b, -c, -v) ---
    if (argc >= 2 && argv[1][0] == '-') {
        std::string cmd = argv[1];

        // 1. VERSION
        if (cmd == "--version" || cmd == "-v") {
            std::wcout << L"Oli Engine v0.2\nBuild Date: " << __DATE__ << std::endl;
            return 0;
        }

        // 2. BUILD STANDALONE (-b)
        if (cmd == "-b") {
            if (argc < 3) return 1;
            std::string inputPath = argv[2];

            // Generăm executabilul în directorul curent de lucru
            std::string outputPath = (argc > 3) ? argv[3] : std::filesystem::path(inputPath).stem().string();
#ifdef _WIN32
            outputPath += ".exe";
#endif

            try {
                std::wcout << L"Oli Engine v0.1\nBuild Date: " << __DATE__ << std::endl;
                std::wcout << L"Building: " << str_to_wstr(inputPath) << std::endl;

                // 🔥 FIX: Citim direct prin UTF-8 helper, eliminând obiectul wif blocant
                std::wstring sourceCode = citeste_fisier_utf8(str_to_wstr(inputPath));
                if (sourceCode.empty()) throw std::runtime_error("Fisier sursa inexistent sau gol.");

                std::wstring upperSource = sourceCode;
                std::transform(upperSource.begin(), upperSource.end(), upperSource.begin(), ::towupper);

                // Verificăm dacă s-a cerut explicit aplicație de Windows GUI
                bool isGuiApp = (upperSource.find(L"CONFIG WIN_APP 1") != std::wstring::npos);

                OliCompiler compiler;
                OliChunk chunk = compiler.compile(sourceCode);

                std::ifstream src(argv[0], std::ios::binary);
                if (!src.is_open()) throw std::runtime_error("Nu s-a putut deschide executabilul sursa al motorului.");

                // Citim peOffset direct din 'src' (oli.exe) înainte de copiere
                uint32_t peOffset = 0;
                if (isGuiApp) {
                    src.seekg(0x3C, std::ios::beg);
                    src.read(reinterpret_cast<char*>(&peOffset), 4);
                    src.seekg(0, std::ios::beg); // Resetăm indexul pentru citirea bufferului
                }

                std::ofstream dst(outputPath, std::ios::binary);
                if (!dst.is_open()) throw std::runtime_error("Nu s-a putut crea executabilul de destinatie.");

                dst << src.rdbuf();
                src.close();

                std::stringstream ss(std::ios::binary | std::ios::out);
                vDataSerialize::serializeChunk(chunk, ss);
                std::string bytecode = ss.str();
                dst.write(bytecode.data(), bytecode.size());

                uint64_t footer = (uint64_t)bytecode.size();
                if (isGuiApp) {
                    footer |= (1ULL << 63);
                    std::wcout << L"[BUILD] Configurare detectata: win_app = 1." << std::endl;
                }

                dst.write(reinterpret_cast<const char*>(&footer), 8);

                // Patch binar rapid pentru subsistemul GUI Windows (Ascundere consolă)
                if (isGuiApp && peOffset > 0) {
                    dst.seekp(peOffset + 0x5C, std::ios::beg);
                    uint16_t subsystemGui = 2; // 2 = IMAGE_SUBSYSTEM_WINDOWS_GUI
                    dst.write(reinterpret_cast<const char*>(&subsystemGui), 2);
                    std::wcout << L"[BUILD] Executabil modificat nativ pentru Windows GUI (Fara consola!)." << std::endl;
                }

                dst.close();

#ifndef _WIN32
                // Pe Linux, setează drepturi de execuție folosind chmod
                chmod(outputPath.c_str(), 0755);
#endif

                std::wcout << L"Building completed." << std::endl;
                return 0;
            }
            catch (const std::exception& e) {
                std::wcerr << L"Build failed: " << str_to_wstr(e.what()) << std::endl;
                return 1;
            }
        }

        // 3. COMPILE BYTECODE (-c) + GENERARE ASSEMBLY (.olia)
        if (cmd == "-c") {
            ConsoleManager::getInstance().setMinLogLevel(LogLevel::DEBUG);
            if (argc < 3) return 1;
            std::string inputPath = argv[2];
            std::string outputPath = (argc > 3) ? argv[3] : inputPath + "c";

            try {
                std::wstring sourceCode = citeste_fisier_utf8(str_to_wstr(inputPath));
                if (sourceCode.empty()) {
                    throw std::runtime_error("Fisier sursa inexistent sau gol.");
                }

                OliCompiler compiler;
                OliChunk chunk = compiler.compile(sourceCode);

                // A. Salvare Bytecode (.olic)
                std::ofstream ofs(outputPath, std::ios::binary);
                if (!ofs.is_open()) {
                    throw std::runtime_error("Nu s-a putut deschide sau crea fisierul de bytecode (.olic).");
                }
                vDataSerialize::serializeChunk(chunk, ofs);
                ofs.close();

                // B. Salvare Assembly Listing (.olia)
                std::wstring assemblyPath = str_to_wstr(outputPath) + L".olia";
                std::string assemblyPathNarrow = PortTools::wstring_to_utf8(assemblyPath);
                std::ofstream asmf(assemblyPathNarrow, std::ios::binary);

                try { asmf.imbue(std::locale("")); }
                catch (...) { asmf.imbue(std::locale::classic()); }

                if (asmf.is_open()) {
                    asmf << "\xEF\xBB\xBF"; 

					// 2. Construim string-ul și îl convertim
					std::wstring content = L"--- OLI ASSEMBLY LISTING ---\n";
					content += L"Source: " + str_to_wstr(inputPath) + L"\n";
					content += L"Generated: 2026-05-11\n\n";
					content += disassembleChunk(chunk);

					// 3. Convertim la UTF-8 folosind funcția ta `utf8_encode` 
					// (pe care o ai deja în ConsoleManager)
					asmf << wstring_to_utf8(content);
                    asmf.close();
                    LOG_SUCCESS(L"Assembly listing generat: " + assemblyPath);
                }
                else {
                    std::wcerr << L"[WARNING] Nu s-a putut genera fisierul de listing .olia (Permisiuni insuficiente)." << std::endl;
                }

                LOG_INFO(L"Compilare reusita: " + str_to_wstr(outputPath));
                return 0;
            }
            catch (const std::exception& e) {
                std::wcerr << L"Compile Error: " << str_to_wstr(e.what()) << std::endl;
                return 1;
            }
        }
    }

    // --- SECTOR B: RULARE SCRIPT SAU SHELL DIRECT ---
    if (argc > 1 && argv[1][0] != '-') {
        std::string scriptPath = argv[1];
        vOliEngine engine;

        if (std::filesystem::path(scriptPath).extension() == ".olic") {
            ConsoleManager::getInstance().setMinLogLevel(LogLevel::LOG_ERROR);
            engine.loadAndRunBytecode(scriptPath);
        }
        else {
            std::wstring scriptContent = citeste_fisier_utf8(str_to_wstr(scriptPath));
            if (scriptContent.empty()) {
                std::wcerr << L"[ERROR] Fisierul sursa '" << str_to_wstr(scriptPath) << L"' nu exista sau este gol." << std::endl;
                return 1;
            }

            std::wstringstream wss(scriptContent);
            std::wstring line;
            while (std::getline(wss, line)) {
                engine.execute(line);
            }
        }
        return 0;
    }

    // --- SECTOR C: SUPORT PIPES (Ex: cat script.oli | oli.exe) ---
    if (!isatty(fileno(stdin))) {
        vOliEngine engine;
        wchar_t buffer[4096];
        while (fgetws(buffer, 4096, stdin)) {
            std::wstring line = buffer;
            if (!line.empty() && line.back() == L'\n') line.pop_back();
            engine.execute(line);
        }
        return 0;
    }

    // --- SECTOR D: INTERFAȚĂ REPL INTERACTIVĂ ---
    oli app(RunMode::CONSOLE);
    app.startConsole();
    return app.run();
}