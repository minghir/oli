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

// Helper function for displaying available CLI options
void showHelp() {
    std::wcout << L"Oli Engine v0.2 - Programming Language Interpreter & Compiler\n";
    std::wcout << L"Usage: oli [options] [script_file]\n\n";
    std::wcout << L"Options:\n";
    std::wcout << L"  -h, --help                     Displays this help message.\n";
    std::wcout << L"  -v, --version                  Displays the engine version and build date.\n";
    std::wcout << L"  -b <input.oli> [-o output]     Compiles the script into a standalone executable (.exe).\n";
    std::wcout << L"  -c <input.oli> [-o output]     Compiles the script to bytecode (.olic) and generates assembly (.olia).\n\n";
    std::wcout << L"Examples:\n";
    std::wcout << L"  oli                             Starts the interactive console (REPL).\n";
    std::wcout << L"  oli script.oli                  Executes an .oli source file.\n";
    std::wcout << L"  oli script.olic                 Executes a compiled binary .olic file.\n";
    std::wcout << L"  oli -b main.oli -o build/app    Generates a native standalone executable.\n";
    std::wcout << L"  oli -b main.oli app.exe         Legacy positional output form.\n";
}

int main(int argc, char* argv[]) {

    ConsoleManager::getInstance().initialize();
    ConsoleManager::getInstance().setMinLogLevel(LogLevel::LOG_ERROR);

    if (vOliEngine::runEmbeddedIfPresent(argv[0])) return 0;

    // --- SECTOR A: FLAGS (-h, -v, -b, -c) ---
    if (argc >= 2 && argv[1][0] == '-') {
        std::string cmd = argv[1];

        // 0. HELP (-h, --help)
        if (cmd == "--help" || cmd == "-h") {
            showHelp();
            return 0;
        }

        // 1. VERSION (-v, --version)
        if (cmd == "--version" || cmd == "-v") {
            std::wcout << L"Oli Engine v0.2\nBuild Date: " << __DATE__ << std::endl;
            return 0;
        }

        // 2. BUILD STANDALONE (-b)
        if (cmd == "-b") {
            std::string inputPath;
            std::string outputPath;

            for (int i = 2; i < argc; ++i) {
                std::string arg = argv[i];

                if (arg == "-o") {
                    if (i + 1 >= argc) {
                        std::wcerr << L"[ERROR] Missing output path for flag -o." << std::endl;
                        std::wcout << L"Usage: oli -b <input.oli> [-o output_file]" << std::endl;
                        return 1;
                    }
                    outputPath = argv[++i];
                    continue;
                }

                if (inputPath.empty()) {
                    inputPath = arg;
                    continue;
                }

                if (outputPath.empty()) {
                    outputPath = arg;
                    continue;
                }

                std::wcerr << L"[ERROR] Unexpected argument: " << str_to_wstr(arg) << std::endl;
                std::wcout << L"Usage: oli -b <input.oli> [-o output_file]" << std::endl;
                return 1;
            }

            if (inputPath.empty()) {
                std::wcerr << L"[ERROR] Missing input file for flag -b." << std::endl;
                std::wcout << L"Usage: oli -b <input.oli> [-o output_file]" << std::endl;
                return 1;
            }

            if (outputPath.empty()) {
                outputPath = std::filesystem::path(inputPath).stem().string();
#ifdef _WIN32
                outputPath += ".exe";
#endif
            }

            try {
                std::wcout << L"Oli Engine v0.2\nBuild Date: " << __DATE__ << std::endl;
                std::wcout << L"Building: " << str_to_wstr(inputPath) << std::endl;

                std::wstring sourceCode = citeste_fisier_utf8(str_to_wstr(inputPath));
                if (sourceCode.empty()) throw std::runtime_error("Source file does not exist or is empty.");

                std::wstring upperSource = sourceCode;
                std::transform(upperSource.begin(), upperSource.end(), upperSource.begin(), ::towupper);

                // Check if Windows GUI application mode was explicitly requested
                bool isGuiApp = (upperSource.find(L"CONFIG WIN_APP 1") != std::wstring::npos);

                OliCompiler compiler;
                OliChunk chunk = compiler.compile(sourceCode);

                std::ifstream src(argv[0], std::ios::binary);
                if (!src.is_open()) throw std::runtime_error("Could not open engine source executable.");

                // Read peOffset directly from 'src' (oli.exe) before copying
                uint32_t peOffset = 0;
                if (isGuiApp) {
                    src.seekg(0x3C, std::ios::beg);
                    src.read(reinterpret_cast<char*>(&peOffset), 4);
                    src.seekg(0, std::ios::beg); // Reset stream index for buffer reading
                }

                std::ofstream dst(outputPath, std::ios::binary);
                if (!dst.is_open()) throw std::runtime_error("Could not create destination executable.");

                dst << src.rdbuf();
                src.close();

                std::stringstream ss(std::ios::binary | std::ios::out);
                vDataSerialize::serializeChunk(chunk, ss);
                std::string bytecode = ss.str();
                dst.write(bytecode.data(), bytecode.size());

                uint64_t footer = (uint64_t)bytecode.size();
                if (isGuiApp) {
                    footer |= (1ULL << 63);
                    std::wcout << L"[BUILD] Detected configuration: win_app = 1." << std::endl;
                }

                dst.write(reinterpret_cast<const char*>(&footer), 8);

                // Fast binary patch for Windows GUI subsystem (Hides console window)
                if (isGuiApp && peOffset > 0) {
                    dst.seekp(peOffset + 0x5C, std::ios::beg);
                    uint16_t subsystemGui = 2; // 2 = IMAGE_SUBSYSTEM_WINDOWS_GUI
                    dst.write(reinterpret_cast<const char*>(&subsystemGui), 2);
                    std::wcout << L"[BUILD] Executable patched for native Windows GUI (Console disabled)." << std::endl;
                }

                dst.close();

#ifndef _WIN32
                // On Linux, set execution permissions using chmod
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

        // 3. COMPILE BYTECODE (-c) + GENERATE ASSEMBLY (.olia)
        if (cmd == "-c") {
            ConsoleManager::getInstance().setMinLogLevel(LogLevel::DEBUG);
            std::string inputPath;
            std::string outputPath;

            for (int i = 2; i < argc; ++i) {
                std::string arg = argv[i];

                if (arg == "-o") {
                    if (i + 1 >= argc) {
                        std::wcerr << L"[ERROR] Missing output path for flag -o." << std::endl;
                        std::wcout << L"Usage: oli -c <input.oli> [-o output_file]" << std::endl;
                        return 1;
                    }
                    outputPath = argv[++i];
                    continue;
                }

                if (inputPath.empty()) {
                    inputPath = arg;
                    continue;
                }

                if (outputPath.empty()) {
                    outputPath = arg;
                    continue;
                }

                std::wcerr << L"[ERROR] Unexpected argument: " << str_to_wstr(arg) << std::endl;
                std::wcout << L"Usage: oli -c <input.oli> [-o output_file]" << std::endl;
                return 1;
            }

            if (inputPath.empty()) {
                std::wcerr << L"[ERROR] Missing input file for flag -c." << std::endl;
                std::wcout << L"Usage: oli -c <input.oli> [-o output_file]" << std::endl;
                return 1;
            }

            if (outputPath.empty()) {
                outputPath = inputPath + "c";
            }

            try {
                std::wstring sourceCode = citeste_fisier_utf8(str_to_wstr(inputPath));
                if (sourceCode.empty()) {
                    throw std::runtime_error("Source file does not exist or is empty.");
                }

                OliCompiler compiler;
                OliChunk chunk = compiler.compile(sourceCode);

                // A. Save Bytecode (.olic)
                std::ofstream ofs(outputPath, std::ios::binary);
                if (!ofs.is_open()) {
                    throw std::runtime_error("Could not open or create the bytecode file (.olic).");
                }
                vDataSerialize::serializeChunk(chunk, ofs);
                ofs.close();

                // B. Save Assembly Listing (.olia)
                std::wstring assemblyPath = str_to_wstr(outputPath) + L".olia";
                std::string assemblyPathNarrow = PortTools::wstring_to_utf8(assemblyPath);
                std::ofstream asmf(assemblyPathNarrow, std::ios::binary);

                try { asmf.imbue(std::locale("")); }
                catch (...) { asmf.imbue(std::locale::classic()); }

                if (asmf.is_open()) {
                    asmf << "\xEF\xBB\xBF"; 

                    std::wstring content = L"--- OLI ASSEMBLY LISTING ---\n";
                    content += L"Source: " + str_to_wstr(inputPath) + L"\n";
                    content += L"Generated: 2026-05-11\n\n";
                    content += disassembleChunk(chunk);

                    asmf << wstring_to_utf8(content);
                    asmf.close();
                    LOG_SUCCESS(L"Assembly listing generated: " + assemblyPath);
                }
                else {
                    std::wcerr << L"[WARNING] Could not generate the assembly listing file .olia (Insufficient permissions)." << std::endl;
                }

                LOG_INFO(L"Compilation successful: " + str_to_wstr(outputPath));
                return 0;
            }
            catch (const std::exception& e) {
                std::wcerr << L"Compile Error: " << str_to_wstr(e.what()) << std::endl;
                return 1;
            }
        }

        // If the flag is unrecognized
        std::wcerr << L"[ERROR] Unknown option: " << str_to_wstr(cmd) << std::endl;
        showHelp();
        return 1;
    }

    // --- SECTOR B: EXECUTE SCRIPT OR SHELL DIRECTLY ---
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
                std::wcerr << L"[ERROR] Source file '" << str_to_wstr(scriptPath) << L"' does not exist or is empty." << std::endl;
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

    // --- SECTOR C: PIPES SUPPORT (Ex: cat script.oli | oli.exe) ---
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

    // --- SECTOR D: INTERACTIVE REPL INTERFACE ---
    oli app(RunMode::CONSOLE);
    app.startConsole();
    return app.run();
}