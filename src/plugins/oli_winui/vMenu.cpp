#include "vMenu.hpp"
#include "vApp.hpp"
#include "ConsoleManager.hpp"
#include "ControlIdManager.hpp"
#include <sstream>

// Constructor
vMenu::vMenu(const std::string& id, EventDispatcher& dispatcher)
    : vControl(nullptr, id, dispatcher), m_handle(NULL) {
   // ConsoleManager::getInstance().log(L"[vMenu::Constructor] Apelat pentru ID: " + std::wstring(id.begin(), id.end()));
}

// Destructor
vMenu::~vMenu() {
    if (m_handle) {
        DestroyMenu(m_handle);
    }
}

// Implementarea metodei virtuale pure create()
/*
void vMenu::create(HWND parent) {
    // Un meniu nu folosește HWND-ul părinte în procesul de creare
    // Dar o poți folosi pentru o eventuală verificare de coerență.

    // Un meniu nu are nevoie de un părinte (HWND) pentru a fi creat.
    // Această metodă are scopul de a îndeplini contractul cu vControl.
    // Codul de creare a meniului rămâne similar cu ce aveai inițial.
    HMENU hMenu = CreateMenu();
    if (!hMenu) {
        ConsoleManager::getInstance().log(L"[ERROR] vMenu::create: Eroare la crearea meniului. Cod eroare: " + std::to_wstring(GetLastError()));
        return;
    }
    m_handle = hMenu;

    // Adăugăm elementele de meniu la handle-ul WinAPI
    for (const auto& item : m_menuItems) {
        if (item.isSeparator) {
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
        }
        else {
            AppendMenu(hMenu, MF_STRING, item.win32Id, item.text.c_str());
        }
    }
  //  ConsoleManager::getInstance().log(L"[vMenu::create] Meniul cu ID '" + std::wstring(m_id.begin(), m_id.end()) + L"' a fost creat cu succes.");
}
*/

void vMenu::create(HWND parent) {
    HMENU hMenu = CreateMenu();
    if (!hMenu) return;
    m_handle = hMenu;

    for (const auto& item : m_menuItems) {
        if (item.isSeparator) {
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
        }
        else {
            UINT flags = MF_STRING;
            if (!item.enabled) flags |= (MF_DISABLED | MF_GRAYED); // Aplică starea aici

            AppendMenuW(hMenu, flags, item.win32Id, item.text.c_str());
        }
    }
}

void vMenu::addItem(const std::string& id, const std::wstring& text) {
    // În acest design, addItem doar stochează elementul în vector
    // Apoi, create() se ocupă de adăugarea lor la meniul WinAPI
    int win32Id = ControlIdManager::allocate(id);
    m_menuItems.push_back({ id, text, win32Id, false });
    if (m_handle) {
        AppendMenuW(m_handle, MF_STRING, win32Id, text.c_str());
    }
   // ConsoleManager::getInstance().log(L"[vMenu::addItem] Added item: '" + text + L"' with internal ID: '" + std::wstring(id.begin(), id.end()) + L"' and Win32 ID: " + std::to_wstring(win32Id));
}

void vMenu::addSeparator(const std::string& id) {
    m_menuItems.push_back({ id, L"", -1, true });
    if (m_handle) {
        AppendMenuW(m_handle, MF_SEPARATOR, 0, NULL);
    }
  //  ConsoleManager::getInstance().log(L"[vMenu::addSeparator] Adăugat separator cu ID: " + std::wstring(id.begin(), id.end()));
}

// Obține ID-ul numeric după ID-ul string
int vMenu::getMenuItemId(const std::string& id) const {
    for (const auto& item : m_menuItems) {
        if (item.id == id) {
            return item.win32Id;
        }
    }
    return -1;
}


std::string vMenu::getItemIdByWin32Id(int win32Id) const {
    for (const auto& item : m_menuItems) {
        if (item.win32Id == win32Id) return item.id;
    }
    return "";
}

void vMenu::addSubMenu(const std::wstring& text, vMenu* subMenu) {
    if (!m_handle || !subMenu || !subMenu->getHandle()) {
        ConsoleManager::getInstance().log(L"[ERROR] vMenu::addSubMenu: Handle-ul meniului părinte sau al submeniului este invalid.");
        return;
    }

    AppendMenuW(
        m_handle,
        MF_POPUP,
        (UINT_PTR)subMenu->getHandle(),
        text.c_str()
    );

   // ConsoleManager::getInstance().log(L"[vMenu::addSubMenu] Adăugat submeniu '" + text + L"' la meniul '" + std::wstring(m_id.begin(), m_id.end()) + L"'.");
}

void vMenu::setEnabled(const std::string& id, bool enabled) {
    // 1. Găsim elementul în vectorul nostru intern pentru a-i actualiza starea
    for (auto& item : m_menuItems) {
        if (item.id == id) {
            item.enabled = enabled;

            // 2. Dacă meniul WinAPI este deja creat, aplicăm schimbarea imediat
            if (m_handle) {
                UINT uEnable = enabled ? MF_ENABLED : (MF_DISABLED | MF_GRAYED);

                // Folosim MF_BYCOMMAND pentru că identificăm elementul prin win32Id (ID-ul numeric)
                EnableMenuItem(m_handle, item.win32Id, MF_BYCOMMAND | uEnable);
            }
            break;
        }
    }
}