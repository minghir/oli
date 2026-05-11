#include "Shell.hpp"
#include "OliEngine.hpp"
#include "App.hpp"
#include "ConsoleManager.hpp"
#include "vDataSerialize.hpp"
#include "OliCompiler.hpp"


#include<iostream>
#include <sstream>
#include <filesystem>
#include <string>
#include <chrono>
#include <iomanip>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

class oli : public App {
public:
    oli( RunMode rm) :App() { setRunMode(rm); };
    ~oli() {};

    bool initConsole() override {
        vOliEngine shClient;
        Shell shell(shClient);
        shell.run();
        return true;
    }

};

int main(int argc, char* argv[]) {
	// Încercăm să rulăm codul embedded dacă există
    // argv[0] este calea către executabilul curent
    if (vOliEngine::runEmbeddedIfPresent(argv[0])) {
        return 0; 
    }
	
	
    ConsoleManager::getInstance().setMinLogLevel(LogLevel::LOG_ERROR);
	
	if (argc >= 2) {
        std::string cmd = argv[1];

        // VERSION
        if (cmd == "--version" || cmd == "-v") {
            std::wcout << L"Oli Engine v0.1\nBuild Date: " << __DATE__ << std::endl;
            return 0;
        }

        // BUILD STANDALONE (-b)
        if (cmd == "-b" && argc >= 3) {
            std::string inputPath = argv[2];
            std::string outputPath = (argc > 3) ? argv[3] : std::filesystem::path(inputPath).stem().string() + ".exe";

            try {
                LOG_INFO(L"Building standalone: " + str_to_wstr(outputPath));
                
                // Refolosim logica de citire sigură (sau funcția readFile dacă o ai)
                std::wifstream wif(inputPath);
                // --- START SAFE IMBUE ---
				try {
					wif.imbue(std::locale("C.UTF-8"));
				} catch (...) {
					try {
						wif.imbue(std::locale("")); // Fallback la sistem
					} catch (...) {
						wif.imbue(std::locale::classic()); // Ultimul recurs
					}
				}
				// --- END SAFE IMBUE ---
                std::wstringstream wss; wss << wif.rdbuf();
                
                OliCompiler compiler;
                OliChunk chunk = compiler.compile(wss.str());

                std::ifstream src(argv[0], std::ios::binary);
                std::ofstream dst(outputPath, std::ios::binary);
                if (!src || !dst) throw std::runtime_error("IO Error");

                dst << src.rdbuf(); // Clonăm motorul
                src.close();

                std::stringstream ss(std::ios::binary | std::ios::out);
                vDataSerialize::serializeChunk(chunk, ss);
                std::string bytecode = ss.str();
                
                dst.write(bytecode.data(), bytecode.size());
                uint64_t footer = (uint64_t)bytecode.size();
                dst.write(reinterpret_cast<const char*>(&footer), 8);
                
                dst.close();
                LOG_SUCCESS(L"Build successful!");
                return 0;
            } catch (const std::exception& e) {
                LOG_ERROR(L"Build failed: " + str_to_wstr(e.what()));
                return 1;
            }
        }
		
		if (argc >= 3 && std::string(argv[1]) == "-c") {
        std::string inputPath = argv[2];
        std::string outputPath = (argc > 3) ? argv[3] : inputPath + "c";

        try {
            ConsoleManager::getInstance().setMinLogLevel(LogLevel::DEBUG);
            LOG_INFO(L"Compiling: " + str_to_wstr(inputPath));

            // Citire sursă
            std::wifstream wif(inputPath);
            try {
				// Încercăm C.UTF-8 (standard pe multe sisteme Linux/MSYS2)
				wif.imbue(std::locale("C.UTF-8"));
			} catch (...) {
				try {
					// Fallback pentru Windows/alte sisteme
					wif.imbue(std::locale("")); 
				} catch (...) {
					// Ultimul recurs: locale-ul clasic (nu va procesa caractere speciale, dar nu crapă)
					wif.imbue(std::locale::classic());
				}
			}
			
			if (!wif.is_open()) {
				LOG_ERROR(L"Nu s-a putut deschide fișierul sursă: " + str_to_wstr(inputPath));
				return 1;
			}

            std::wstringstream wss;
            wss << wif.rdbuf();
            std::wstring source = wss.str();

            // Compilare
            OliCompiler compiler;
            OliChunk chunk = compiler.compile(source);

            // Salvare Bytecode
            std::ofstream ofs(outputPath, std::ios::binary);
            if (!ofs.is_open()) {
                LOG_ERROR(L"Nu s-a putut deschide fisierul de iesire.");
                return 1;
            }
            vDataSerialize::serializeChunk(chunk, ofs);
            ofs.close();

            LOG_INFO(L"[SUCCESS] Bytecode salvat in: " + str_to_wstr(outputPath));
            
            // Opțional: Generare Assembly automată la compilare
            std::wstring assemblyPath = str_to_wstr(outputPath) + L".olia";
            if (ConsoleManager::getInstance().enableFileLogging(assemblyPath, false)) {
                ConsoleManager::getInstance().writeRaw(L"--- OLI ASSEMBLY ---\n" + disassembleChunk(chunk));
                ConsoleManager::getInstance().closeLogFile();
            }

            return 0; // Ieșim după compilare
        }
        catch (const std::exception& e) {
            std::wcerr << L"Compile Error: " << e.what() << std::endl;
            return 1;
        }
    }
	}
	
	
	
	
	
	
	
	
	
	
	
	
	
	
    // 1. Detectăm dacă STDIN este terminal sau pipe
    bool stdin_is_terminal = isatty(fileno(stdin));
	// 1. Verificăm dacă avem flag de compilare: oli.exe -c fisier.oli [iesire.olic]
    
	
    // 2. Dacă avem un argument (script.oli) → rulăm acel fișier
    if (argc > 1) {
        std::string scriptPath = argv[1];

        // Verificăm dacă fișierul există
        if (!std::filesystem::exists(scriptPath)) {
            std::wcerr << L"[ERROR] Script file not found: "
                << std::filesystem::path(scriptPath).wstring() << L"\n";
            return 1;
        }

        vOliEngine engine;
        std::filesystem::path p(scriptPath);
        std::string extension = p.extension().string();

        // --- LOGICA NOUĂ PENTRU BYTECODE ---
        if (extension == ".olic") {
            // Dacă extensia este .olic, apelăm direct VM-ul
            engine.loadAndRunBytecode(scriptPath);
        }
        else {
            // Altfel, rămânem pe interpretarea clasică (linie cu linie)
            std::wifstream file(scriptPath);
            try {
                file.imbue(std::locale("en_US.UTF-8"));
            }
            catch (...) {
                file.imbue(std::locale::classic());
            }

            std::wstring line;
            while (std::getline(file, line)) {
                engine.execute(line);
            }
        }

        return 0;
    }

    // 3. Dacă STDIN vine din pipe → rulăm în modul batch
    if (!stdin_is_terminal) {
        
        vOliEngine engine;
        std::wstring line;

        while (true) {
            wchar_t buffer[4096];
            if (!fgetws(buffer, 4096, stdin))
                break;

            line = buffer;
            if (!line.empty() && line.back() == L'\n')
                line.pop_back();

            engine.execute(line);
        }
        return 0;
    }
    
    // 4. Altfel → modul interactiv normal
    
    oli app(RunMode::CONSOLE);
    app.startConsole();
    
    return app.run();
}
