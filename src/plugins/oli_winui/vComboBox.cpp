// vComboBox.cpp

#include "vComboBox.hpp"
#include "ConsoleManager.hpp" // Pentru logare

// Constructor
vComboBox::vComboBox(HINSTANCE hInstance, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher, int dropdownHeight)
    : vControl(hInstance, id, x, y, width, height, dispatcher),
    m_dropdownHeight(dropdownHeight)
     {
   // ConsoleManager::getInstance().log(L"[vComboBox::Constructor] Apelat pentru ID: " + std::wstring(id.begin(), id.end()));
}

// Metoda create
void vComboBox::create(HWND parent) {
   // ConsoleManager::getInstance().log(L"[vComboBox::create] Creare ComboBox cu ID: " + std::wstring(m_id.begin(), m_id.end()) + L" în părinte HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(parent)));

    if (!parent) {
        ConsoleManager::getInstance().log(L"[ERROR] vComboBox::create: Părintele HWND este nullptr.");
        return;// false;
    }

    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);
    if (!hInstance) {
        ConsoleManager::getInstance().log(L"[ERROR] vComboBox::create: Nu s-a putut obține HINSTANCE de la părinte.");
        return;// false;
    }

    
    if (m_height <= 0)
        m_height = 28;   // înălțime normală pentru zona de afișare
    
    m_handle = CreateWindowExW(
        0,
        L"COMBOBOX",
        nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | CBS_HASSTRINGS | CBS_OWNERDRAWFIXED,
        m_x, m_y,
        m_width,
        m_height,   // <-- AICI trebuie pusă înălțimea zonei de afișare
        parent,
        (HMENU)(uintptr_t)getWin32Id(),
        hInstance,
        this
    );
    
    /*
    m_handle = CreateWindowEx(
        0,
        L"COMBOBOX",
        nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | CBS_OWNERDRAWFIXED, // <-- ADĂUGAT
        m_x, m_y, m_width, m_dropdownHeight,
        parent,
        (HMENU)(uintptr_t)getWin32Id(),
        hInstance,
        this
    );
    */

    ::SendMessageW(m_handle, CB_SETITEMHEIGHT, (WPARAM)-1, (LPARAM)m_height);
    ::SendMessageW(m_handle, CB_SETITEMHEIGHT, (WPARAM)0, (LPARAM)m_height);

    if (!m_handle) {
        ConsoleManager::getInstance().log(L"[ERROR] Creare vComboBox: Eroare la crearea HWND-ului cu ID '" + std::wstring(m_id.begin(), m_id.end()) + L"'. Cod eroare: " + std::to_wstring(GetLastError()));
    }
    else {
        GetClientRect(m_handle, &m_originalClientRect);
        UINT parentDpi = GetDpiForWindow(parent);
        this->scale(parentDpi);

        // --- ADAUGĂ ACESTE LINII ---
        // Setează înălțimea pentru elementele din listă
        ::SendMessage(m_handle, CB_SETITEMHEIGHT, 0, (LPARAM)m_height);
        // Setează înălțimea pentru câmpul principal de selecție (WPARAM -1)
        ::SendMessage(m_handle, CB_SETITEMHEIGHT, (WPARAM)-1, (LPARAM)m_height);

        // Asigură-te că controlul are fontul corect aplicat la nivel de sistem
        if (m_hFont) {
            ::SendMessage(m_handle, WM_SETFONT, (WPARAM)m_hFont, TRUE);

            ::InvalidateRect(m_handle, NULL, TRUE);
            ::UpdateWindow(m_handle);
        }
    }
    return;// m_handle != nullptr;
}

// Gestionarea mesajelor
LRESULT vComboBox::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    //LOG_DEBUG(L"Combo box command");
    switch (msg) {
    case WM_COMMAND: {
        int controlID = LOWORD(wParam);
        int notificationCode = HIWORD(wParam);

        if (controlID == getWin32Id()) {
            switch (notificationCode) {
            case CBN_SELCHANGE: {
                //LOG_DEBUG(L"[vComboBox::handleMessage] CBN_SELCHANGE detectat pentru ID: " + std::wstring(m_id.begin(), m_id.end()));
                onSelectionChange(); // Declanșează metoda specifică
                return 0; // Am gestionat mesajul
            }
                              // Adaugă alte notificări dacă este necesar (ex: CBN_DROPDOWN)
            }
        }
        break; // Lasă WM_COMMAND să fie procesat de vControl pentru copii
    }
    case WM_MEASUREITEM: {
        LPMEASUREITEMSTRUCT lpMIS = (LPMEASUREITEMSTRUCT)lParam;
        if (lpMIS->CtlID == (UINT)getWin32Id()) {
            // înălțimea unui item (listă + zona de afișare)
            lpMIS->itemHeight = m_height > 0 ? m_height : 24;
            return TRUE;
        }
        break;
    }
   

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT)lParam;
        if (lpDIS->CtlID == (UINT)getWin32Id()) {
            HDC hdc = lpDIS->hDC;
            RECT rect = lpDIS->rcItem;
            int itemID = lpDIS->itemID;

            // Nu avem ce desena dacă indexul e invalid
            if (itemID < 0) return TRUE;

            bool isSelected = (lpDIS->itemState & ODS_SELECTED);
            COLORREF bgColor = isSelected ? GetSysColor(COLOR_HIGHLIGHT) : m_backgroundColor;
            COLORREF txtColor = isSelected ? GetSysColor(COLOR_HIGHLIGHTTEXT) : m_textColor;

            // Desenăm fundalul
            HBRUSH hBrush = ::CreateSolidBrush(bgColor);
            ::FillRect(hdc, &rect, hBrush);
            ::DeleteObject(hBrush);

            // Extragere text
            std::wstring itemText = L"";
            int len = (int)::SendMessageW(lpDIS->hwndItem, CB_GETLBTEXTLEN, itemID, 0);
            if (len != CB_ERR && len > 0) {
                std::vector<wchar_t> buffer(len + 1, 0);
                ::SendMessageW(lpDIS->hwndItem, CB_GETLBTEXT, itemID, (LPARAM)buffer.data());
                itemText = buffer.data();
            }

            if (!itemText.empty()) {
                ::SetBkMode(hdc, TRANSPARENT);
                ::SetTextColor(hdc, txtColor);

                HFONT hFontToUse = m_hFont ? m_hFont : (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
                HGDIOBJ oldFont = ::SelectObject(hdc, hFontToUse);

                RECT textRect = rect;
                textRect.left += 5;

                ::DrawTextW(hdc, itemText.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                ::SelectObject(hdc, oldFont);
            }

            if ((lpDIS->itemState & ODS_FOCUS) && !(lpDIS->itemState & ODS_NOFOCUSRECT)) {
                ::DrawFocusRect(hdc, &rect);
            }
            return TRUE;
        }
        break;
    }
    default:
        break;
    }
    return vControl::handleMessage(hwnd, msg, wParam, lParam);
}
/*
void vComboBox::addItem(const std::wstring& text, LPARAM itemData) {
    if (m_handle) {
        // Folosește SendMessageW pentru a forța tratarea textului ca Unicode
        LRESULT index = ::SendMessageW(m_handle, CB_ADDSTRING, 0, (LPARAM)text.c_str());
        if (index != CB_ERR && itemData != 0) {
            ::SendMessageW(m_handle, CB_SETITEMDATA, index, itemData);
        }
    }
}
*/
void vComboBox::addItem(const std::wstring& text, LPARAM itemData) {
    if (m_handle) {
        // CB_ADDSTRING returnează indexul unde a fost inserat string-ul
        LRESULT index = ::SendMessageW(m_handle, CB_ADDSTRING, 0, (LPARAM)text.c_str());

        if (index != CB_ERR) {
            // Trimitem datele indiferent dacă sunt 0 sau nu
            ::SendMessageW(m_handle, CB_SETITEMDATA, (WPARAM)index, itemData);
        }
    }
}


void vComboBox::clearItems() {
    if (m_handle) {
        SendMessage(m_handle, CB_RESETCONTENT, 0, 0);
       // ConsoleManager::getInstance().log(L"[vComboBox::clearItems] ComboBox golit pentru ID: " + std::wstring(m_id.begin(), m_id.end()));
    }
}

int vComboBox::getSelectedIndex() const {
    if (m_handle) {
        return static_cast<int>(SendMessage(m_handle, CB_GETCURSEL, 0, 0));
    }
    return CB_ERR; // -1 în caz de eroare sau niciun element selectat
}

std::wstring vComboBox::getSelectedText() const {
    int index = getSelectedIndex();
    if (m_handle && index != CB_ERR) {
        // Obține lungimea textului
        int len = static_cast<int>(SendMessage(m_handle, CB_GETLBTEXTLEN, index, 0));
        if (len != CB_ERR) {
            // Alocă spațiu și obține textul
            std::vector<wchar_t> buffer(len + 1); // +1 pentru null terminator
            SendMessage(m_handle, CB_GETLBTEXT, index, (LPARAM)buffer.data());
            return std::wstring(buffer.data());
        }
    }
    return L"";
}

LPARAM vComboBox::getSelectedItemData() const {
    int index = getSelectedIndex();
    if (m_handle && index != CB_ERR) {
        return SendMessage(m_handle, CB_GETITEMDATA, index, 0);
    }
    return 0; // Sau o altă valoare de eroare/null
}

void vComboBox::setSelectedIndex(int index) {
    if (m_handle) {
        SendMessage(m_handle, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
        // Trimite un mesaj CBN_SELCHANGE manual dacă vrei să declanșezi evenimentul,
        // dar de obicei se presupune că setarea programatică nu necesită notificare.
        // SendMessage(GetParent(m_handle), WM_COMMAND, MAKEWPARAM(getWin32Id(), CBN_SELCHANGE), (LPARAM)m_handle);
    }
}

// Implementarea onSelectionChange pentru a declanșa evenimentul prin EventDispatcher
void vComboBox::onSelectionChange() {
    LOG_DEBUG(L"[vComboBox::onSelectionChange] Schimbare selecție ComboBox pentru ID: " + std::wstring(m_id.begin(), m_id.end()));
    //getEventDispatcher().dispatch("selectionChange"); // Declanșează evenimentul "selectionChange"
    getEventDispatcher().dispatch("selectionChange",m_id); // Declanșează evenimentul "selectionChange"
    //getEventDispatcher().dispatch("selectionChange",m_id); // Declanșează evenimentul "selectionChange"
}

void vComboBox::scale(int newDpi) {
    vControl::scale(newDpi); // Scalează m_width și m_height (cele de layout, ex: 30px)
    this->scaleFont(newDpi);

    if (m_handle) {
        // Recalculăm înălțimea totală pentru noul monitor
        int scaledFullHeight = MulDiv(m_dropdownHeight, newDpi, 96);

        SetWindowPos(m_handle, NULL, 0, 0,
            m_width,        // Lățimea deja scalată în vControl::scale
            scaledFullHeight,
            SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}


void vComboBox::setText(const std::wstring& text) {
    if (!m_handle) return;

    // 1. Dacă textul e gol, deselectăm totul și ieșim fără eroare
    if (text.empty() || text == L" ") {
        ::SendMessageW(m_handle, CB_SETCURSEL, (WPARAM)-1, 0);
        return;
    }

    // 2. Căutăm string-ul exact
    int index = (int)::SendMessageW(m_handle, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)text.c_str());

    if (index != CB_ERR) {
        setSelectedIndex(index);
        // Validăm vizual selecția
        ::InvalidateRect(m_handle, NULL, TRUE);
    }
    else {
        // 3. Logăm eroarea doar dacă chiar aveam ce să căutăm
        ConsoleManager::getInstance().log(L"[vComboBox] setText failed: '" + text + L"' nu a fost găsit în listă.");
    }
}