#include "xApp.hpp"
#include "IXLayoutStrategy.hpp"
#include "../../ConsoleManager.hpp"
#include <iostream>

// Inițializăm instanța statică a singleton-ului
xApp* xApp::s_instance = nullptr;

xApp::xApp(RunMode mode) : m_runMode(mode) {
    s_instance = this;
}

bool xApp::init() {
    // 1. Inițializare GTK standard (echivalentul pregătirii subsistemului grafic)
    // Pasăm parametri goli (0, nullptr) deoarece GTK permite inițializarea fără argc/argv din main.
    if (!gtk_init_check(0, nullptr)) {
        ConsoleManager::getInstance().log(L"[xApp] EROARE: Nu s-a putut inițializa GTK!");
        return false;
    }

    // 2. Rutare în funcție de modul de rulare stabilit de motorul Oli
    switch (m_runMode) {
        case RunMode::GUI:
            return initGui();
        case RunMode::CONSOLE:
            return initConsole();
        case RunMode::SERVICE:
            return initService();
    }
    return false;
}

bool xApp::initGui() {
    ConsoleManager::getInstance().log(L"[xApp] Subsistemul Grafic GTK inițializat cu succes.");
    return true;
}

int xApp::run() {
    ConsoleManager::getInstance().log(L"[xApp] Se pornește bucla principală GTK (gtk_main)...");
    
    // 🔥 Aceasta înlocuiește vechea buclă de mesaje Win32 (GetMessage/DispatchMessage).
    // Funcția blochează execuția și randează interfața până când se apelează gtk_main_quit().
    gtk_main();
    
    return 0;
}

void xApp::shutdown() {
    ConsoleManager::getInstance().log(L"[xApp] Oprire aplicație și distrugere ferestre.");
    
    // Oprim bucla GTK dacă este activă
    if (gtk_main_level() > 0) {
        gtk_main_quit();
    }
}

void xApp::handleGlobalSignal(GtkWidget* /*widget*/, const std::string& /*signalName*/) {
    // Hook pentru semnale globale la nivel de aplicație
}

GtkWidget* xApp::getMainWindow() {
    // Returnează handle-ul primei ferestre active din manager
    xWindow* mainWin = m_windowManager.getFirstWindow();
    return mainWin ? mainWin->getHandle() : nullptr;
}

xWindow* xApp::getWindow(const std::string& id) {
    return m_windowManager.get(id);
}

void xApp::addWindow(const std::string& id, std::unique_ptr<xWindow> window) {
    m_windowManager.add(id, std::move(window));
}

void xApp::removeWindow(const std::string& id) {
    m_windowManager.remove(id);
}

void xApp::startConsole() {
    ConsoleManager::getInstance().log(L"[xApp] Consola de debug pornită.");
}

void xApp::test() {
    // Funcție utilitară goală pentru teste interne rapide
}