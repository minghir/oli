#include "StringUtils.hpp"
#include "Shell.hpp"
#include "ConsoleManager.hpp"

#include <cwctype>

#ifdef _WIN32
#include <conio.h>  // _kbhit(), _getwch()
#include <fcntl.h>
#include <io.h>
#endif

    Shell::Shell(IShellEngine& engine) : m_engine(engine), m_running(true) {}
    
    void Shell::run() {
        LOG_SUCCESS(L"--- Shell Interface Started ---");

        while (!m_engine.shouldExit()) {
            std::wcout << m_engine.getPrompt();

            std::wstring line;
            if (!std::getline(std::wcin, line)) break;
            m_engine.execute(line);
        }
    }
    