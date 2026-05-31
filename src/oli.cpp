#include "Shell.hpp"
#include "OliEngine.hpp"
#include "App.hpp"
#include "ConsoleManager.hpp"
#include "vDataSerialize.hpp"
#include "OliCompiler.hpp"
#include "StringUtils.hpp" 
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

// Funcție utilitară pentru a evita eroarea "bad locale name" pe Windows
static void safe_imbue(std::wifstream& file) {
    try {
        file.imbue(std::locale(""));
    }
    catch (...) {
        try {
            file.imbue(std::locale("C.UTF-8"));
        }
        catch (...) {
            file.imbue(std::locale::classic());
        }
    }
}

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
            std::wcout << L"Oli Engine v0.1\nBuild Date: " << __DATE__ << std::endl;
            return 0;
        }

        // 2. BUILD STANDALONE (-b)
        /*
        if (cmd == "-b") {
            if (argc < 3) return 1;
            std::string inputPath = argv[2];
            std::string outputPath = (argc > 3) ? argv[3] : std::filesystem::path(inputPath).stem().string() + ".exe";

            try {
                std::wcout << L"Oli Engine v0.1\nBuild Date: " << __DATE__ << std::endl;
                std::wcout << L"Buildding: " << str_to_wstr(inputPath) << std::endl;
                std::wifstream wif(inputPath);
                safe_imbue(wif);
                if (!wif.is_open()) throw std::runtime_error("Fisier sursa inexistent.");

                std::wstringstream wss; wss << wif.rdbuf();
                OliCompiler compiler;
                OliChunk chunk = compiler.compile(wss.str());

                std::ifstream src(argv[0], std::ios::binary);
                std::ofstream dst(outputPath, std::ios::binary);
                dst << src.rdbuf();
                src.close();

                std::stringstream ss(std::ios::binary | std::ios::out);
                vDataSerialize::serializeChunk(chunk, ss);
                std::string bytecode = ss.str();
                dst.write(bytecode.data(), bytecode.size());
                uint64_t footer = (uint64_t)bytecode.size();
                dst.write(reinterpret_cast<const char*>(&footer), 8);
                dst.close();
                std::wcout << L"Buildding completed. " << std::endl;
                return 0;
            }
            catch (const std::exception& e) {
                std::wcerr << L"Build failed: " << str_to_wstr(e.what()) << std::endl;
                return 1;
            }
        }
        */
        /*
        // 2. BUILD STANDALONE (-b)
        if (cmd == "-b") {
            if (argc < 3) return 1;
            std::string inputPath = argv[2];

            // 🔥 MODIFICARE AICI: Păstrăm calea completă și doar schimbăm extensia în .exe
            std::filesystem::path p(inputPath);
            p.replace_extension(".exe");
            std::string outputPath = (argc > 3) ? argv[3] : p.string();

            try {
                std::wcout << L"Oli Engine v0.1\nBuild Date: " << __DATE__ << std::endl;
                std::wcout << L"Buildding: " << str_to_wstr(inputPath) << std::endl;
                std::wifstream wif(inputPath);
                safe_imbue(wif);
                if (!wif.is_open()) throw std::runtime_error("Fisier sursa inexistent.");

                std::wstringstream wss; wss << wif.rdbuf();
                OliCompiler compiler;
                OliChunk chunk = compiler.compile(wss.str());

                std::ifstream src(argv[0], std::ios::binary);
                std::ofstream dst(outputPath, std::ios::binary); // Acum dst va folosi calea completă a scriptului!
                dst << src.rdbuf();
                src.close();

                std::stringstream ss(std::ios::binary | std::ios::out);
                vDataSerialize::serializeChunk(chunk, ss);
                std::string bytecode = ss.str();
                dst.write(bytecode.data(), bytecode.size());
                uint64_t footer = (uint64_t)bytecode.size();
                dst.write(reinterpret_cast<const char*>(&footer), 8);
                dst.close();
                std::wcout << L"Buildding completed. " << std::endl;
                return 0;
            }
            catch (const std::exception& e) {
                std::wcerr << L"Build failed: " << str_to_wstr(e.what()) << std::endl;
                return 1;
            }
        }
        */
        // 2. BUILD STANDALONE (-b)
        /*
        if (cmd == "-b") {
            if (argc < 3) return 1;
            std::string inputPath = argv[2];

            // 🔥 MODIFICARE: Folosim .stem() pentru a genera .exe-ul în directorul curent de lucru,
            // astfel încât să aibă acces direct la DLL-urile pluginurilor din folder!
            std::string outputPath = (argc > 3) ? argv[3] : std::filesystem::path(inputPath).stem().string() + ".exe";

            try {
                std::wcout << L"Oli Engine v0.1\nBuild Date: " << __DATE__ << std::endl;
                std::wcout << L"Building: " << str_to_wstr(inputPath) << std::endl;
                std::wifstream wif(inputPath);
                safe_imbue(wif);
                if (!wif.is_open()) throw std::runtime_error("Fisier sursa inexistent.");

                std::wstringstream wss; wss << wif.rdbuf();
                std::wstring sourceCode = wss.str();

                // 🔥 O facem UPPERCASE pentru a fi siguri că prindem și "config win_app 1" și "CONFIG WIN_APP 1"
                std::wstring upperSource = sourceCode;
                std::transform(upperSource.begin(), upperSource.end(), upperSource.begin(), ::towupper);

                // 🎯 CĂUTARE EXPLICITĂ: Verificăm dacă ai cerut explicit aplicație de Windows
                bool isGuiApp = (upperSource.find(L"CONFIG WIN_APP 1") != std::wstring::npos);

                OliCompiler compiler;
                OliChunk chunk = compiler.compile(sourceCode);

                std::ifstream src(argv[0], std::ios::binary);
                std::ofstream dst(outputPath, std::ios::binary);
                dst << src.rdbuf();
                src.close();

                std::stringstream ss(std::ios::binary | std::ios::out);
                vDataSerialize::serializeChunk(chunk, ss);
                std::string bytecode = ss.str();
                dst.write(bytecode.data(), bytecode.size());

                // Păstrăm trucul cu bitul 63 în footer pentru consistența VM-ului
                uint64_t footer = (uint64_t)bytecode.size();
                if (isGuiApp) {
                    footer |= (1ULL << 63); // Activăm flag-ul și în footer-ul intern
                    std::wcout << L"[BUILD] Configurare detectata: win_app = 1." << std::endl;
                }

                dst.write(reinterpret_cast<const char*>(&footer), 8);
                dst.close(); // 🔥 Închidem stream-ul inițial de scriere ca să eliberăm fișierul

                std::wcout << L"Building completed. " << std::endl;

                // =================================================================
                // 🔥 ABORDAREA PROFESIONALĂ: Patch la PE Header pentru Subsistem GUI
                // =================================================================
                if (isGuiApp) {
                    // Deschidem fișierul proaspăt generat în mod citire + scriere binară
                    std::fstream fs(outputPath, std::ios::in | std::ios::out | std::ios::binary);
                    if (fs.is_open()) {
                        // 1. Citim locația PE header (e_lfanew) aflată la offset-ul fix 0x3C în formatul MZ
                        fs.seekg(0x3C, std::ios::beg);
                        uint32_t peOffset = 0;
                        fs.read(reinterpret_cast<char*>(&peOffset), 4);

                        // 2. Câmpul Subsystem se află exact la adresa: peOffset + 0x5C
                        fs.seekp(peOffset + 0x5C, std::ios::beg);
                        uint16_t subsystemGui = 2; // 2 = IMAGE_SUBSYSTEM_WINDOWS_GUI (3 era Console)

                        fs.write(reinterpret_cast<const char*>(&subsystemGui), 2);
                        fs.close();
                        std::wcout << L"[BUILD] Executabil modificat nativ pentru Windows GUI (Fara consola!)." << std::endl;
                    }
                    else {
                        std::wcerr << L"[BUILD] Avertisment: Nu s-a putut deschide .exe pentru patch-ul PE." << std::endl;
                    }
                }

                return 0;
            }
            catch (const std::exception& e) {
                std::wcerr << L"Build failed: " << str_to_wstr(e.what()) << std::endl;
                return 1;
            }
        }
        */
        // 2. BUILD STANDALONE (-b)
// 2. BUILD STANDALONE (-b)
if (cmd == "-b") {
    if (argc < 3) return 1;
    std::string inputPath = argv[2];

    // Generăm .exe-ul în directorul curent de lucru
    std::string outputPath = (argc > 3) ? argv[3] : std::filesystem::path(inputPath).stem().string() + ".exe";

    try {
        std::wcout << L"Oli Engine v0.1\nBuild Date: " << __DATE__ << std::endl;
        std::wcout << L"Building: " << str_to_wstr(inputPath) << std::endl;
        std::wifstream wif(inputPath);
        safe_imbue(wif);
        if (!wif.is_open()) throw std::runtime_error("Fisier sursa inexistent.");

        std::wstringstream wss; wss << wif.rdbuf();
        std::wstring sourceCode = wss.str();

        std::wstring upperSource = sourceCode;
        std::transform(upperSource.begin(), upperSource.end(), upperSource.begin(), ::towupper);

        // Verificăm dacă s-a cerut explicit aplicație de Windows GUI
        bool isGuiApp = (upperSource.find(L"CONFIG WIN_APP 1") != std::wstring::npos);

        OliCompiler compiler;
        OliChunk chunk = compiler.compile(sourceCode);

        std::ifstream src(argv[0], std::ios::binary);
        if (!src.is_open()) throw std::runtime_error("Nu s-a putut deschide executabilul sursa al motorului.");

        // =================================================================
        // 🔥 TRUC DE GENIU: Citim peOffset direct din 'src' (oli.exe)
        // =================================================================
        uint32_t peOffset = 0;
        if (isGuiApp) {
            src.seekg(0x3C, std::ios::beg);
            src.read(reinterpret_cast<char*>(&peOffset), 4);
            src.seekg(0, std::ios::beg); // Resetăm la început pentru rdbuf()
        }

        // 🔥 Păstrăm dst ca std::ofstream pur (Garantat va crea fișierul din prima!)
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

        // =================================================================
        // 🔥 PATCH VIA SEEKP: Modificăm bitul PE direct în stream-ul de scriere
        // =================================================================
        if (isGuiApp && peOffset > 0) {
            dst.seekp(peOffset + 0x5C, std::ios::beg);
            uint16_t subsystemGui = 2; // 2 = IMAGE_SUBSYSTEM_WINDOWS_GUI (Fara consola)
            dst.write(reinterpret_cast<const char*>(&subsystemGui), 2);
            std::wcout << L"[BUILD] Executabil modificat nativ pentru Windows GUI (Fara consola!)." << std::endl;
        }

        dst.close(); // Închidem abia acum, curat și complet
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
                // Setăm log-ul la DEBUG pentru a vedea ce se întâmplă în consolă
                //ConsoleManager::getInstance().setMinLogLevel(LogLevel::DEBUG);

                std::wifstream wif(inputPath);
                safe_imbue(wif);
                std::wstringstream wss; wss << wif.rdbuf();

                OliCompiler compiler;
                OliChunk chunk = compiler.compile(wss.str());

                // A. Salvare Bytecode (.olic)
                std::ofstream ofs(outputPath, std::ios::binary);
                vDataSerialize::serializeChunk(chunk, ofs);
                ofs.close();

                // B. Salvare Assembly Listing (.olia)
                std::wstring assemblyPath = str_to_wstr(outputPath) + L".olia";
                std::wofstream asmf(assemblyPath.c_str());

                // Folosim același safe_imbue pentru scriere
                try { asmf.imbue(std::locale("")); }
                catch (...) { asmf.imbue(std::locale::classic()); }

                if (asmf.is_open()) {
                    asmf << L"--- OLI ASSEMBLY LISTING ---\n";
                    asmf << L"Source: " << str_to_wstr(inputPath) << L"\n";
                    asmf << L"Generated: " << L"2026-05-11\n\n"; // Hardcoded pentru contextul tău

                    // Apelăm funcția ta de dezasamblare
                    asmf << disassembleChunk(chunk);

                    asmf.close();
                    LOG_SUCCESS(L"Assembly listing generat: " + assemblyPath);
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

    // --- SECTOR B: RULARE SCRIPT SAU SHELL ---
    if (argc > 1 && argv[1][0] != '-') {
        std::string scriptPath = argv[1];
        vOliEngine engine;
        if (std::filesystem::path(scriptPath).extension() == ".olic") {
			ConsoleManager::getInstance().setMinLogLevel(LogLevel::LOG_ERROR);
            engine.loadAndRunBytecode(scriptPath);
        }
        else {
            std::wifstream file(scriptPath);
            safe_imbue(file);
            std::wstring line;
            while (std::getline(file, line)) engine.execute(line);
        }
        return 0;
    }

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

    oli app(RunMode::CONSOLE);
    app.startConsole();
    return app.run();
}