#include "vMenu.hpp"
#include "vApp.hpp"
#include "../../ConsoleManager.hpp"
#include "ControlIdManager.hpp"
#include <sstream>
#include <algorithm>

// Constructor
vMenu::vMenu(const std::string& id, EventDispatcher& dispatcher)
    : vControl(nullptr, id, dispatcher), m_handle(NULL) {
}

// Destructor
vMenu::~vMenu() {
    if (m_handle) {
        DestroyMenu(m_handle);
    }
}

// 🔥 Helper local pentru a converti stringul literal "\\t" într-un tab real L'\t'
static std::wstring ConvertEscapeSequences(const std::wstring& text) {
    std::wstring result = text;
    size_t pos = 0;
    // Căutăm secvența de 2 caractere: backslash și 't'
    while ((pos = result.find(L"\\t", pos)) != std::wstring::npos) {
        result.replace(pos, 2, L"\t"); // Înlocuim cu caracterul real de tabulare
        pos += 1; // Avansăm cursorul
    }
    return result;
}

void vMenu::create(HWND parent) {
    if (m_handle) return;

    HMENU hMenu = CreateMenu();
    if (!hMenu) {
        LOG_ERROR(L"[vMenu::create] Eroare critica: CreateMenu a returnat NULL!");
        return;
    }

    m_handle = hMenu;

    // Populare...
    for (const auto& item : m_menuItems) {
        if (item.isSeparator) {
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        }
        else {
            UINT flags = MF_STRING;
            if (!item.enabled) flags |= (MF_DISABLED | MF_GRAYED);
            AppendMenuW(hMenu, flags, item.win32Id, item.text.c_str());
        }
    }
    LOG_DEBUG(L"[vMenu::create] Meniu creat cu succes. HMENU: " + std::to_wstring(reinterpret_cast<uintptr_t>(m_handle)));
}

void vMenu::addItem(const std::string& id, const std::wstring& text) {
    // 🔥 Convertim textul din script pentru a suporta tab-ul nativ Win32
    std::wstring cleanText = ConvertEscapeSequences(text);

    int win32Id = ControlIdManager::allocate(id);
    m_menuItems.push_back({ id, cleanText, win32Id, false });

    if (m_handle) {
        AppendMenuW(m_handle, MF_STRING, win32Id, cleanText.c_str());
    }
}

void vMenu::addSeparator(const std::string& id) {
    m_menuItems.push_back({ id, L"", -1, true });
    if (m_handle) {
        AppendMenuW(m_handle, MF_SEPARATOR, 0, NULL);
    }
}

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
    if (!m_handle || !subMenu || !subMenu->getHandle()) return;

    // 🔥 Convertim textul și pentru submeniuri (în caz că pui scurtături pe textul principal)
    std::wstring cleanText = ConvertEscapeSequences(text);

    AppendMenuW(
        m_handle,
        MF_POPUP,
        (UINT_PTR)subMenu->getHandle(),
        cleanText.c_str()
    );
}

void vMenu::setEnabled(const std::string& id, bool enabled) {
    for (auto& item : m_menuItems) {
        if (item.id == id) {
            item.enabled = enabled;

            if (m_handle) {
                UINT uEnable = enabled ? MF_ENABLED : (MF_DISABLED | MF_GRAYED);
                EnableMenuItem(m_handle, item.win32Id, MF_BYCOMMAND | uEnable);
            }
            break;
        }
    }
}

bool vMenu::setProperty(const std::wstring& name, const vData& value) {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    if (prop == L"enabled_item") {
        return true;
    }
    return vControl::setProperty(name, value);
}

vData vMenu::getProperty(const std::wstring& name) const {
    return vControl::getProperty(name);
}

bool vMenu::callMethod(const std::wstring& methodName, const std::vector<vData>& args) {
    if (methodName.empty()) {
        LOG_ERROR(L"[vMenu::callMethod] EROARE: methodName este GOL!");
        return false;
    }

    std::wstring method = methodName;
    std::transform(method.begin(), method.end(), method.begin(), ::tolower);

    LOG_DEBUG(L"[vMenu::callMethod] Metoda primita: '" + method + L"', Nr arg: " + std::to_wstring(args.size()));

    if (method == L"add_item") {
        if (args.size() < 2) {
            LOG_ERROR(L"[vMenu::callMethod] Eroare: add_item necesita cel putin 2 argumente (id, text).");
            return false;
        }

        std::wstring wItemId = args[0].toWString();
        std::string itemId(wItemId.begin(), wItemId.end());
        std::wstring itemText = args[1].toWString();

        LOG_DEBUG(L"[vMenu::callMethod] Execut addItem. ID: " + wItemId + L", Text: " + itemText);
        this->addItem(itemId, itemText);
        return true;
    }
    else if (method == L"add_separator") {
        if (args.empty()) {
            LOG_ERROR(L"[vMenu::callMethod] Eroare: add_separator necesita ID.");
            return false;
        }

        std::wstring wSepId = args[0].toWString();
        std::string sepId(wSepId.begin(), wSepId.end());

        LOG_DEBUG(L"[vMenu::callMethod] Execut addSeparator. ID: " + wSepId);
        this->addSeparator(sepId);
        return true;
    }
    else if (method == L"add_submenu") {
        if (args.size() < 2) {
            LOG_ERROR(L"[vMenu::callMethod] Eroare: add_submenu necesita 2 argumente (text, childMenuId).");
            return false;
        }

        std::wstring subMenuText = args[0].toWString();
        std::wstring wChildMenuId = args[1].toWString();
        std::string childMenuId(wChildMenuId.begin(), wChildMenuId.end());

        LOG_DEBUG(L"[vMenu::callMethod] Execut addSubMenu. Text: " + subMenuText + L", ChildID: " + wChildMenuId);

        extern vControl* LocateAnyControl(const std::string & id);
        vControl* ctrl = LocateAnyControl(childMenuId);
        vMenu* childMenu = dynamic_cast<vMenu*>(ctrl);

        if (!childMenu) {
            LOG_ERROR(L"[vMenu::callMethod] Eroare: Submeniul cu ID '" + wChildMenuId + L"' nu a fost gasit sau nu e de tip vMenu!");
            return false;
        }

        this->addSubMenu(subMenuText, childMenu);
        return true;
    }

    LOG_ERROR(L"[vMenu::callMethod] Eroare: Metoda '" + method + L"' nu este implementata.");
    return false;
}