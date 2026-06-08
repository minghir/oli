#include "../../OliEngine.hpp"
#include "../../ConsoleManager.hpp"
#include "../../OliCompiler.hpp"
#include "../../vDataSerialize.hpp"
#include <fstream>
#include <filesystem>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#include <limits.h>
#endif

// --- 1. DEFINIȚIE EXPORT ---
#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
    #define OLI_EXPORT extern "C" __declspec(dllexport)
#else
    #define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#endif

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

OLI_EXPORT void SetPluginConsoleManager(ConsoleManager* hostCm) {
    ConsoleManager::setInstance(hostCm);
}

void RegisterCompilerFunctions(PluginRegistry& registry) {
    // --- 1. FUNCȚIA COMPILE ---
    registry[L"COMPILE"] = [](const std::vector<vData>& args) -> vData {
		if (args.empty()) return vData(L"ERR_NO_FILE");

		std::wstring inputPath = args[0].toWString();
		std::wstring outputPath = (args.size() > 1) ? args[1].toWString() : inputPath + L"c";

		// 🔥 FIX: Salvăm nivelul vechi și setăm INFO/DEBUG pentru ca mesajele să ajungă în UI
		LogLevel oldLevel = ConsoleManager::getInstance().getLogLevel();
		ConsoleManager::getInstance().setMinLogLevel(LogLevel::DEBUG); 

		LOG_INFO(L"[COMPILE] START");
		
		try {
			std::wstring sourceCode = citeste_fisier_utf8(inputPath);
			if (sourceCode.empty()) throw std::runtime_error("Fisier sursa inexistent sau gol.");

			OliCompiler compiler;
			OliChunk chunk = compiler.compile(sourceCode);

			std::string narrowOut = wstr_to_str(outputPath);
			std::ofstream ofs(narrowOut, std::ios::binary);
			if (!ofs.is_open()) throw std::runtime_error("Nu s-a putut crea .olic");
			vDataSerialize::serializeChunk(chunk, ofs);
			ofs.close();

			std::string narrowAsm = wstr_to_str(outputPath + L".olia");
			std::ofstream asmf(narrowAsm, std::ios::binary);
			
			if (asmf.is_open()) {
				asmf << "\xEF\xBB\xBF";
				std::wstring listing = disassembleChunk(chunk);
				asmf << utf8_encode(listing); 
				asmf.close();
				LOG_SUCCESS(L"Assembly listing generat: " + str_to_wstr(narrowAsm));
			} else {
				LOG_ERROR(L"Nu s-a putut genera fisierul de listing .olia");
			}

			LOG_SUCCESS(L"Compilare finalizată cu succes.");
			
			// 🔥 FIX: Restaurăm nivelul original înainte de return
			ConsoleManager::getInstance().setMinLogLevel(oldLevel);
			return vData(L"SUCCESS");
		}
		// Ne asigurăm că restaurăm nivelul chiar dacă apare o excepție (catch)
		catch (const std::exception& e) {
			std::string err = e.what();
			LOG_ERROR(L"Compile Error: " + std::wstring(err.begin(), err.end()));
			
			// 🔥 FIX: Restaurăm nivelul original în caz de eroare
			ConsoleManager::getInstance().setMinLogLevel(oldLevel);
			return vData(L"ERR_COMPILE");
		}
	};

	// --- 2. FUNCȚIA BUILD ---
    // --- 2. FUNCȚIA BUILD RESCRISĂ (CURATĂ ȘI PORTABILĂ) ---
    registry[L"BUILD"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 1) return vData(L"ERR_ARGS");

        // Permitem logurile în timpul build-ului
        LogLevel oldLevel = ConsoleManager::getInstance().getLogLevel();
        ConsoleManager::getInstance().setMinLogLevel(LogLevel::INFO);

        std::wstring inputPath = args[0].toWString();
        std::wstring outputPath;

        // Dacă utilizatorul nu trimite o cale explicită ca al doilea argument,
        // generăm noi automat numele curat în directorul curent de lucru
        if (args.size() > 1) {
            outputPath = args[1].toWString();
        } else {
            // std::filesystem::path(inputPath).stem() extrage doar numele (ex: "aa" din "tests/aa.oli")
            std::filesystem::path p(inputPath);
            outputPath = p.stem().wstring() + L".exe"; 
        }

        // 🔥 FIX ARHITECTURAL: Aflăm calea IDE-ului curent (olide.exe) - cross-platform
        std::filesystem::path ideFullPath;
    #ifdef _WIN32
        wchar_t currentIdePath[MAX_PATH];
        GetModuleFileNameW(NULL, currentIdePath, MAX_PATH);
        ideFullPath = std::filesystem::path(currentIdePath);
    #else
        char currentIdePath[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", currentIdePath, sizeof(currentIdePath) - 1);
        if (len != -1) {
            currentIdePath[len] = '\0';
            ideFullPath = std::filesystem::path(std::string(currentIdePath));
        } else {
            ideFullPath = std::filesystem::current_path();
        }
    #endif

        // Extragem directorul bazei și forțăm țintirea către motorul curat 'oli.exe'
        std::filesystem::path baseDir = ideFullPath.parent_path();
        std::filesystem::path cleanEnginePath = baseDir / "oli.exe";

        // Verificăm dacă motorul curat există lângă IDE înainte de a continua
        if (!std::filesystem::exists(cleanEnginePath)) {
            LOG_ERROR(L"Build Error: Nu s-a găsit motorul curat (oli.exe) la calea: " + cleanEnginePath.wstring());
            ConsoleManager::getInstance().setMinLogLevel(oldLevel);
            return vData(L"ERR_NO_ENGINE");
        }

        try {
            std::wstring sourceCode = citeste_fisier_utf8(inputPath);
            if (sourceCode.empty()) throw std::runtime_error("Sursa goala sau inexistenta.");

            bool isGuiApp = (sourceCode.find(L"CONFIG WIN_APP 1") != std::wstring::npos);

            OliCompiler compiler;
            OliChunk chunk = compiler.compile(sourceCode);

            // Convertim căile finale în std::string pentru funcțiile standard I/O
            std::string narrowExePath = wstr_to_str(cleanEnginePath.wstring()); // 🔥 Copiem oli.exe, NU olide.exe!
            std::string narrowOutputPath = wstr_to_str(outputPath);

            // Copiere executabil motor curat (oli.exe -> aa.exe)
            std::filesystem::copy_file(narrowExePath, narrowOutputPath, std::filesystem::copy_options::overwrite_existing);

            // Deschidem executabilul destinație în mod append pentru a lipi noul bytecode curat
            std::ofstream dst(narrowOutputPath, std::ios::binary | std::ios::app);
            if (!dst.is_open()) throw std::runtime_error("Eroare la deschiderea executabilului destinatie.");
            
            std::stringstream ss(std::ios::binary | std::ios::out);
            vDataSerialize::serializeChunk(chunk, ss);
            std::string bytecode = ss.str();
            dst.write(bytecode.data(), bytecode.size());

            uint64_t footer = (uint64_t)bytecode.size();
            if (isGuiApp) footer |= (1ULL << 63);
            dst.write(reinterpret_cast<const char*>(&footer), 8);
            dst.close();

            // Patching GUI (folosind corect seekg pentru citire și seekp pentru scriere)
            if (isGuiApp) {
                std::fstream patcher(narrowOutputPath, std::ios::binary | std::ios::in | std::ios::out);
                if (patcher.is_open()) {
                    // Folosim seekg (Get pointer) pentru operațiunea de citire (read)
                    patcher.seekg(0x3C, std::ios::beg);
                    uint32_t peOffset = 0;
                    patcher.read(reinterpret_cast<char*>(&peOffset), 4);
                    
                    // Siguranță: Verificăm că am citit un offset valid din PE Header
                    if (peOffset > 0 && peOffset < 1024) { 
                        // Folosim seekp (Put pointer) pentru operațiunea de scriere (write)
                        patcher.seekp(peOffset + 0x5C, std::ios::beg);
                        uint16_t subsystemGui = 2; // IMAGE_SUBSYSTEM_WINDOWS_GUI
                        patcher.write(reinterpret_cast<const char*>(&subsystemGui), 2);
                    } else {
                        LOG_WARNING(L"[BUILD] Nu s-a putut aplica patch-ul GUI: PE offset invalid.");
                    }
                    patcher.close();
                }
            }

            LOG_SUCCESS(L"Build complet realizat cu succes: " + outputPath);
            
            // Restaurăm nivelul original de log
            ConsoleManager::getInstance().setMinLogLevel(oldLevel);
            return vData(L"SUCCESS");
        }
        catch (const std::exception& e) {
            LOG_ERROR(L"Build Error: " + str_to_wstr(e.what()));
            
            // Restaurăm nivelul original în caz de eroare
            ConsoleManager::getInstance().setMinLogLevel(oldLevel);
            return vData(L"ERR_BUILD");
        }
    };
}

OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {
    RegisterCompilerFunctions(registry);
}