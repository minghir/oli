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
        std::wstring line;

        // Tot sistemul de input, istoricul și conversiile sunt aici:
        if (!PortTools::getConsoleInput(m_engine.getPrompt(), line)) {
            break;
        }

        if (!line.empty()) {
            m_engine.execute(line);
        }
    }

}