
#include "App.hpp"

// Inițializează pointerul static al instanței în afara clasei.
App* App::s_instance = nullptr;

// --- Constructor ---
App::App( RunMode mode)
     {
    s_instance = this;
    m_runMode = mode;
}

// --- Metoda Run ---
int App::run() {
    if (!init()) {
        ConsoleManager::getInstance().log(L"[ERROR] Application initialization has failed.");
        return -1;
    }
    if (m_runMode == RunMode::CONSOLE) {
        return 0;
    }

    return -1;
}

bool App::init() {

    // Ramificarea logicii
    switch (m_runMode) {
    case RunMode::CONSOLE:
        return initConsole(); // Apează inițializarea Console
    default:
        ConsoleManager::getInstance().log(L"[ERROR] Unknown mode.");
        return false;
    }
}

// --- Metoda Shutdown ---
void App::shutdown() {
    ConsoleManager::getInstance().log(L"[App::shutdown] Se inițiază procedura de închidere...");
    ConsoleManager::getInstance().clearExtraOutputs();
    ConsoleManager::getInstance().log(L"[App::shutdown] Resurse eliberate.");
    ConsoleManager::getInstance().shutdown();
}

void App::startConsole() {
   ConsoleManager::getInstance().initialize(); 
   ConsoleManager::getInstance().setColor(FOREGROUND_GREEN);
   //ConsoleManager::getInstance().log(L"Consola inițializată! [AppInit] Începe inițializarea aplicației...");
   ConsoleManager::getInstance().resetColor();
   
   
}
