#include "stringUtils.hpp"
#include "vShell.hpp"
#include "ConsoleManager.hpp"

#include <cwctype>

#include <conio.h> // Pentru _kbhit() și _getwch()
#include <fcntl.h>
#include <io.h>

    vShell::vShell(IShellEngine& engine) : m_engine(engine), m_running(true) {}
    
    void vShell::run() {
        LOG_SUCCESS(L"--- Shell Interface Started ---");

        while (!m_engine.shouldExit()) {
            std::wcout << m_engine.getPrompt();

            std::wstring line;
            if (!std::getline(std::wcin, line)) break;
            //line = normalizeSpaces(line);
            //LOG_INFO(L"vShell::run("+line+L")");
            m_engine.execute(line);
        }
    }
    