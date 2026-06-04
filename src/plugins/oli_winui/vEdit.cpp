#include "vEdit.hpp"
#include "../../ConsoleManager.hpp" // Pentru logare
#include "FontManager.hpp" // Pentru logare
#include "stringUtils.hpp"

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")



LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    vEdit* pEdit = reinterpret_cast<vEdit*>(dwRefData);

    switch (uMsg) {

    case WM_CHAR: {
        // Exemplu: Dacă vrem doar cifre și Backspace
        if (pEdit->isNumericOnly()) {
            if (!iswdigit((wchar_t)wParam) && wParam != VK_BACK) {
                // Sunet de avertizare (opțional) și blocare caracter
                MessageBeep(MB_ICONINFORMATION);
                return 0;
            }
        }

        // Dacă vrei un format personalizat (ex: litere mari forțate)
        // wParam = towupper((wchar_t)wParam); 

        break;
    }

    case WM_KILLFOCUS: {
        // Apelăm validarea direct aici, este cel mai sigur punct
        pEdit->validate();
        pEdit->getEventDispatcher().dispatch("lost_focus", pEdit->getId());
        break; // Lăsăm și Windows-ul să-și facă treaba de focus
    }
    case WM_GETDLGCODE: {
        // Dacă este MultiLine, lăsăm comportamentul default al Windows-ului 
        // (Windows știe deja că MultiLine vrea Enter pentru rând nou)
        if (pEdit->getEditType() == EditType::MULTI_LINE) {
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }

        // Dacă este SingleLine (Filtru), aplicăm logica noastră de interceptare
        MSG* pMsg = (MSG*)lParam;
        if (pMsg && pMsg->message == WM_KEYDOWN) {
            if (pMsg->wParam == VK_RETURN) return DLGC_WANTALLKEYS;
            if (pMsg->wParam == VK_TAB) return 0;
        }
        return DLGC_WANTCHARS | DLGC_WANTARROWS;
    }

    case WM_KEYDOWN:
        if (wParam == VK_RETURN &&  pEdit->getEditType() == EditType::SINGLE_LINE ) {
            pEdit->getEventDispatcher().dispatch("lost_focus", pEdit->getId());
            SetFocus(GetParent(hWnd));
            return 0;
        }
        else if (wParam == VK_RETURN && pEdit->getEditType() == EditType::CONSOLE_LINE) {
            pEdit->getEventDispatcher().dispatch("lost_focus", pEdit->getId());
            
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
            
        }
        break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}


// Constructor - Inițializează membrii clasei
vEdit::vEdit(HINSTANCE hInstance, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher, EditType type)
    : vControl(hInstance, id, x, y, width, height, dispatcher),
    m_editType(type)
   {
    //ConsoleManager::getInstance().log(L"[vEdit::Constructor] Apelat pentru ID: " + std::wstring(id.begin(), id.end()));
}

// Metoda create - creează controlul EDIT
void vEdit::create(HWND parent) {
    //ConsoleManager::getInstance().log(L"[vEdit::create] Creare Edit Box cu ID: " + std::wstring(m_id.begin(), m_id.end()) + L" în părinte HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(parent)));

    if (!parent) {
        //ConsoleManager::getInstance().log(L"[ERROR] vEdit::create: Părintele HWND este nullptr. Edit Box-ul nu poate fi creat.");
        return;
    }

    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);
    if (!hInstance) {
        //ConsoleManager::getInstance().log(L"[ERROR] vEdit::create: Nu s-a putut obține HINSTANCE de la părinte.");
        return;
    }
    UINT parentDpi = GetDpiForWindow(parent);
    // Apelează metoda de scalare cu DPI-ul obținut
    scale(parentDpi);


    DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | WS_CLIPSIBLINGS;
    if (m_onlyDigits) {
        dwStyle |= ES_NUMBER;
    }

    if (m_editType == EditType::MULTI_LINE || m_editType == EditType::CONSOLE_LINE) {
        // Adăugăm stilurile pentru multiline
        dwStyle |= (ES_MULTILINE | ES_WANTRETURN | WS_VSCROLL | ES_AUTOVSCROLL);
        // Scoatem scroll-ul orizontal automat dacă vrem wrap pe text
        dwStyle &= ~ES_AUTOHSCROLL;
    }
    else if (m_editType == EditType::PASSWORD) {
        dwStyle |= ES_PASSWORD;

    }

    if (m_isReadOnly) {
        dwStyle |= ES_READONLY;
    }

    m_handle = CreateWindowExW(
        WS_EX_CLIENTEDGE,                // Stiluri extinse: adaugă o margine 3D
        L"EDIT",                         // Clasa de fereastră standard pentru controale editabile
        L"",                             // Textul inițial este gol
        dwStyle,
        m_x, m_y, m_width, m_height,    // Poziție și dimensiune
        parent,                          // Fereastra părinte
        (HMENU)(uintptr_t)getWin32Id(),  // ID-ul controlului
        hInstance,                       // Handle-ul instanței
        this                             // Pointer către obiectul vEdit
    );

    if (m_handle) {
        ::EnableWindow(m_handle, m_enabled ? TRUE : FALSE);

        // --- NOU: Adaugă logică pentru scalarea fontului ---
        // 1. Creează un font scalat, folosind o dimensiune de bază (ex: 12) și DPI-ul.
        HFONT scaledFont = FontManager::getInstance().getScaledFont(L"Arial", 12, parentDpi);

        // 2. Aplică fontul pe controlul vEdit.
        SendMessage(m_handle, WM_SETFONT, (WPARAM)scaledFont, TRUE);

        // Asigură-te că fontul va fi șters la distrugerea controlului
        // (nu este arătat aici, dar este o bună practică).
        SetWindowSubclass(m_handle, EditSubclassProc, 0, reinterpret_cast<DWORD_PTR>(this));

        //ConsoleManager::getInstance().log(L"[vEdit::create] Edit Box-ul (ID: " + std::wstring(m_id.begin(), m_id.end()) + L") a fost creat cu succes. HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(m_handle)));
    }
    else {
        ConsoleManager::getInstance().log(L"[ERROR] Creare vEdit: Eroare la crearea HWND-ului. Cod eroare: " + std::to_wstring(GetLastError()));
    }
}

// Gestionarea mesajelor - similar cu vControl, dar poate fi extins
LRESULT vEdit::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Pentru vEdit, de obicei nu este nevoie de o gestionare complexă a mesajelor în wrapper.
    // Mesajele precum WM_COMMAND sunt procesate de fereastra părinte.
    switch (msg) {
   
    

        case WM_COMMAND: {
            int notificationCode = HIWORD(wParam);
            if (notificationCode == EN_KILLFOCUS) {
                //m_dispatcher.dispatch("lost_focus", m_id);
                return 0;
            }
            break;
        }
    }
    return vControl::handleMessage(hwnd, msg, wParam, lParam);
}

// Setează textul în controlul WinAPI
void vEdit::setText(const std::wstring& newText) {
    if (m_handle) {
        SetWindowTextW(m_handle, newText.c_str());
       // ConsoleManager::getInstance().log(L"[vEdit::setText] Textul pentru ID: " + std::wstring(m_id.begin(), m_id.end()) + L" a fost setat la: '" + newText + L"'");
    }
    else {
        ConsoleManager::getInstance().log(L"[ERROR] vEdit::setText: HWND invalid pentru ID: " + std::wstring(m_id.begin(), m_id.end()));
    }
}

// Obține textul curent din controlul WinAPI
std::wstring vEdit::getText() const {
    if (m_handle) {
        int length = GetWindowTextLength(m_handle);
        if (length > 0) {
            std::vector<wchar_t> buffer(length + 1);
            GetWindowTextW(m_handle, buffer.data(), length + 1);
            return std::wstring(buffer.data());
        }
    }
    return L""; // Returnează un șir gol dacă handle-ul nu este valid sau textul este gol
}
/*
void vEdit::onKillFocus() {
    //ConsoleManager::getInstance().log(L"[vEdit::onKillFocus] A fost pierdut focusul de pe " + str_to_wstr(m_id));
    getEventDispatcher().dispatch("lost_focus", m_id);
}
*/
void vEdit::setReadOnly(bool bReadOnly) {
    m_isReadOnly = bReadOnly;
    if (m_handle) {
        // EM_SETREADONLY: wParam este TRUE pentru read-only, FALSE pentru editabil
        SendMessage(m_handle, EM_SETREADONLY, (WPARAM)(bReadOnly ? TRUE : FALSE), 0);

      //  ConsoleManager::getInstance().log(L"[vEdit::setReadOnly] Status setat la " +
      //      std::wstring(bReadOnly ? L"READ-ONLY" : L"EDITABIL") +
       //     L" pentru ID: " + str_to_wstr(m_id));
    }
    else {
        ConsoleManager::getInstance().log(L"[ERROR] vEdit::setReadOnly: HWND invalid pentru ID: " + str_to_wstr(m_id));
    }
}


void vEdit::setEditType(EditType type) {
    m_editType = type;

    if (m_handle) {
        long style = GetWindowLong(m_handle, GWL_STYLE);

        if (type == EditType::PASSWORD) {
            // Adăugăm stilul de parolă
            SetWindowLong(m_handle, GWL_STYLE, style | ES_PASSWORD);
            // Setăm caracterul de mascare (implicit un punct sau asterisc conform temei Windows)
            SendMessage(m_handle, EM_SETPASSWORDCHAR, (WPARAM)L'●', 0);
        }
        else {
            // Eliminăm stilul de parolă pentru celelalte tipuri
            SetWindowLong(m_handle, GWL_STYLE, style & ~ES_PASSWORD);
            SendMessage(m_handle, EM_SETPASSWORDCHAR, 0, 0);
        }

        // Forțăm controlul să se redeseneze pentru a aplica vizual modificarea
        SetWindowPos(m_handle, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        InvalidateRect(m_handle, nullptr, TRUE);
    }
}