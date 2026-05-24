#include "../../OliEngine.hpp"
#include <thread>
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <cstdlib>
#endif

#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#include <windows.h>
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#include <unistd.h>
#endif

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

// --- Helper pentru conversii ---
inline std::wstring vDataToWString(const vData& v) {
    return v.toWString(); // Presupunând că vData are această metodă
}

void RegisterSystemFunctions(PluginRegistry& registry) {

    // --- EXEC: Blocant (Așteaptă finalizarea, bun pentru compilare) ---
    registry[L"SYS"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(L"");
        std::wstring command = args[0].toWString();

        std::wstring output;

        // Pe Windows, folosim _wpopen pentru a deschide un pipe către comanda dată
        FILE* pipe = _wpopen(command.c_str(), L"r");
        if (!pipe) return vData(L"ERR_PIPE_FAILED");

        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            // Convertim ANSI (sau ce scoate compilatorul) în wstring
            output += std::wstring(buffer, buffer + strlen(buffer));
        }

        _pclose(pipe);
        return vData(output); // Returnăm tot output-ul ca string
        };

    // --- START: Non-blocant (Lansează proces separat, bun pentru .exe-uri) ---
    /*
    registry[L"START_PROCESS"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(0LL);
        std::wstring path = vDataToWString(args[0]);

#if defined(_WIN32) || defined(_WIN64)
        // ShellExecute returnează imediat, nu blochează UI-ul
        HINSTANCE hInst = ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return vData((long long)((uintptr_t)hInst > 32));
#else
        // Implementare simplă pentru Unix
        return vData((long long)(system((std::string(path.begin(), path.end()) + " &").c_str()) == 0));
#endif
        };
        */

    registry[L"START_PROCESS"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(0LL);
        std::wstring path = vDataToWString(args[0]);

        // Structuri necesare pentru CreateProcess
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        // 🔥 Flag-ul magic: STARTF_USESHOWWINDOW + SW_HIDE pentru a nu deschide consolă
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_SHOWNORMAL; // Fereastra aplicației tale va fi normală

        // Convertim wstring la un buffer mutabil pentru CreateProcess
        std::vector<wchar_t> cmdLine(path.begin(), path.end());
        cmdLine.push_back(0);

        // CreateProcess oferă control total asupra procesului creat
        BOOL success = CreateProcessW(
            NULL,           // Aplicația (NULL dacă e în cmdLine)
            cmdLine.data(), // Linia de comandă
            NULL,           // Process handle nu e moștenit
            NULL,           // Thread handle nu e moștenit
            FALSE,          // Nu moștenim handle-uri
            //CREATE_NEW_CONSOLE, // Aici poți încerca DETACHED_PROCESS pentru a evita consola
			DETACHED_PROCESS, // Flag pentru a nu deschide o consolă nouă
            NULL,           // Mediu de lucru
            NULL,           // Director curent
            &si,            // Startup info
            &pi             // Process info
        );

        if (success) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return vData(1LL);
        }
        else {
            return vData(0LL);
        }
        };

    // --- THREAD_RUN: Execuție asincronă pentru logică internă (dacă ai nevoie) ---
    registry[L"THREAD_RUN"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(0LL);

        // Notă: Aceasta este pentru a lansa un thread care rulează ceva, 
        // dar atenție: nu modifica UI-ul direct din acest thread!
        std::wstring cmd = vDataToWString(args[0]);
        std::thread([cmd]() {
            _wsystem(cmd.c_str());
            }).detach();

        return vData(1LL);
        };
}

OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {
    RegisterSystemFunctions(registry);
}