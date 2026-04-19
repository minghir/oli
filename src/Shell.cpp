#include "StringUtils.hpp"
#include "Shell.hpp"
#include "ConsoleManager.hpp"
#include "PortTools.hpp" // Avem nevoie de conversii
#include <cwctype>

#ifdef _WIN32
#include <conio.h>
#include <fcntl.h>
#include <io.h>
#else
#include <iostream>
#include <readline/readline.h>
#include <readline/history.h>
#endif

Shell::Shell(IShellEngine& engine) : m_engine(engine), m_running(true) {}

void Shell::run() {
    LOG_SUCCESS(L"--- Shell Interface Started ---");

    while (!m_engine.shouldExit()) {
        std::wstring prompt = m_engine.getPrompt();

#ifdef _WIN32
        // --- WINDOWS: getline e de ajuns pentru CMD/PowerShell ---
        std::wcout << prompt;
        std::wstring line;
        if (!std::getline(std::wcin, line)) break;
        m_engine.execute(line);
#else
        // --- LINUX: Folosim GNU Readline pentru săgeți și istoric ---
        // Readline lucrează cu char* (UTF-8)
        std::string promptA = PortTools::wstring_to_utf8(prompt);
        char* input = readline(promptA.c_str());

        if (!input) break; // Ctrl+D pentru exit

        if (*input) {
            add_history(input); // Aceasta activează "săgeata sus"
        }

        std::wstring line = PortTools::utf8_to_wstring(input);
        free(input); // Eliberăm memoria alocată de readline

        m_engine.execute(line);
#endif
    }
}