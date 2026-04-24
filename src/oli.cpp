#include "Shell.hpp"
#include "OliEngine.hpp"
#include "App.hpp"


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

    // 1. Detectăm dacă STDIN este terminal sau pipe
    bool stdin_is_terminal = isatty(fileno(stdin));

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

        // Citim fișierul linie cu linie
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
