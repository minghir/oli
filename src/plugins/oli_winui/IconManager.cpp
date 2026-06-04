#include "IconManager.hpp"
#include "../../ConsoleManager.hpp" // Presupunem că folosești această clasă pentru logare

// Inițializarea statică a instanței
IconManager& IconManager::getInstance() {
    static IconManager instance;
    return instance;
}

// Destructorul - responsabil cu eliberarea resurselor
IconManager::~IconManager() {
    cleanup();
}

HICON IconManager::getIcon(const std::wstring& filePath, int width, int height) {
    IconKey key = { filePath, width, height };

    // Caută pictograma în cache
    auto it = m_iconCache.find(key);
    if (it != m_iconCache.end()) {
        ConsoleManager::getInstance().log(L"[IconManager] Returnează pictogramă existentă: " + filePath);
        return it->second;
    }

    // Pictograma nu există, încarcă-o din fișier
    HICON hIcon = (HICON)LoadImageW(
        NULL,           // Instanță (NULL pentru încărcare din fișier)
        filePath.c_str(), // Calea către fișierul .ico
        IMAGE_ICON,     // Tipul de imagine (icon)
        width,          // Lățimea dorită
        height,         // Înălțimea dorită
        LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED
    );

    if (hIcon) {
        m_iconCache[key] = hIcon; // Adaugă pictograma în cache
        ConsoleManager::getInstance().log(L"[IconManager] Creat și adăugat pictogramă nouă din fișier: " + filePath);
    }
    else {
        ConsoleManager::getInstance().log(L"[ERROR] IconManager: Nu s-a putut încărca pictograma din fișier: " + filePath + L". Eroare: " + std::to_wstring(GetLastError()));
    }

    return hIcon;
}

void IconManager::cleanup() {
    ConsoleManager::getInstance().log(L"[IconManager] Curățare resurse pictograme...");
    for (auto const& [key, hIcon] : m_iconCache) {
        if (hIcon) {
            DestroyIcon(hIcon); // Eliberează resursa GDI
        }
    }
    m_iconCache.clear(); // Golește cache-ul
    ConsoleManager::getInstance().log(L"[IconManager] Toate pictogramele au fost eliberate.");
}