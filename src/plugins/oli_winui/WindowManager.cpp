#include "WindowManager.hpp"
#include "ConsoleManager.hpp"
// #include <iostream> // Nu este necesar dacă folosești ConsoleManager

// --- Metoda Add ---
void WindowManager::add(const std::string& id, std::unique_ptr<vWindow> win) {
   // ConsoleManager::getInstance().log(L"[WindowManager::add] Adaug fereastra cu ID: " + std::wstring(id.begin(), id.end()) + L", HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(win->getHandle())));
    m_windows[id] = std::move(win);
}

// --- Metoda Get ---
vWindow* WindowManager::get(const std::string& id) {
    auto it = m_windows.find(id);
    //if (it == m_windows.end()) LOG_ERROR(L"NU GASES:" + str_to_wstr(id));
    //else LOG_ERROR(L"AM GASIT returnez:" + str_to_wstr(id));
    return (it != m_windows.end()) ? it->second.get() : nullptr;
}

// --- Metoda GetWindowByHandle (Nouă) ---
vWindow* WindowManager::getWindowByHandle(HWND hwnd) {
    // Iterează prin toate ferestrele gestionate
    for (const auto& pair : m_windows) {
        // Accesează pointerul brut la vWindow
        vWindow* currentWindow = pair.second.get();
        // Verifică dacă HWND-ul acestei ferestre corespunde cu cel căutat
        if (currentWindow && currentWindow->getHandle() == hwnd) {
           // ConsoleManager::getInstance().log(L"[WindowManager::getWindowByHandle] Fereastră găsită pentru HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(hwnd)) + L", ID: " + std::wstring(pair.first.begin(), pair.first.end()));
            return currentWindow; // Am găsit fereastra, returnăm pointerul
        }
    }
   // ConsoleManager::getInstance().log(L"[WindowManager::getWindowByHandle] Nicio fereastră găsită pentru HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(hwnd)));
    return nullptr; // Nu s-a găsit nicio fereastră cu HWND-ul specificat
}

// --- Metoda Remove ---
void WindowManager::remove(const std::string& id) {
   // ConsoleManager::getInstance().log(L"[WindowManager::remove] Elimin fereastra cu ID: " + std::wstring(id.begin(), id.end()));
    m_windows.erase(id);
}

// --- Metoda Shutdown ---
void WindowManager::shutdown() {
   // ConsoleManager::getInstance().log(L"[WindowManager::shutdown] Inițiez oprirea managerului de ferestre...");

    for (auto& pair : m_windows) {
        const std::string& id = pair.first;
        vWindow* win = pair.second.get();
        if (win) {
           // ConsoleManager::getInstance().log(L"[WindowManager::shutdown] Închiderea ferestrei: " + std::wstring(id.begin(), id.end()) + L" (HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(win->getHandle())) + L")");
        }
    }

    m_windows.clear();
   // ConsoleManager::getInstance().log(L"[WindowManager::shutdown] Toate ferestrele au fost închise și managerul golit.");
}