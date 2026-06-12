#include "XWindowManager.hpp"
#include "IXLayoutStrategy.hpp"
#include "../../ConsoleManager.hpp"
#include "../../StringUtils.hpp"


// --- Metoda Add ---
void XWindowManager::add(const std::string& id, std::unique_ptr<xWindow> win) {
    m_windows[id] = std::move(win);
}

// --- Metoda Get ---
xWindow* XWindowManager::get(const std::string& id) {
    auto it = m_windows.find(id);
    return (it != m_windows.end()) ? it->second.get() : nullptr;
}

// --- Metoda GetWindowByHandle ---
xWindow* XWindowManager::getWindowByHandle(GtkWidget* widget) {
    if (!widget) return nullptr;

    // Iterăm prin ferestrele active pentru a asocia evenimentele asincrone din GTK cu obiectul corect
    for (const auto& pair : m_windows) {
        xWindow* currentWindow = pair.second.get();
        if (currentWindow && currentWindow->getHandle() == widget) {
            return currentWindow;
        }
    }
    return nullptr;
}

// --- Metoda Remove ---
void XWindowManager::remove(const std::string& id) {
    m_windows.erase(id);
}

// --- Metoda Shutdown ---
void XWindowManager::shutdown() {
    LOG_INFO(L"[XWindowManager::shutdown] Inițiez curățarea structurală a ferestrelor active...");

    // Ne asigurăm că eliberarea memoriei nu lasă widget-uri GTK "agățate" în backend
    m_windows.clear();
    
    LOG_SUCCESS(L"[XWindowManager::shutdown] Toate obiectele xWindow au fost distruse cu succes.");
}