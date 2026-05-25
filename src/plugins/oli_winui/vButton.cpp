#include "vButton.hpp"
#include "ConsoleManager.hpp"
#include "IconManager.hpp" 
#include "TooltipManager.hpp" 
#include "stringUtils.hpp"

#include <algorithm>
#include <windows.h> // Pentru funcții WinAPI

// --- Constructor ---
vButton::vButton(HINSTANCE hInstance, const std::string& id, const std::wstring& label, int x, int y, int width, int height, EventDispatcher& dispatcher)
// Aici inițializezi membrii vControl, inclusiv poziția și dimensiunea
    : vControl(hInstance, id, x, y, width, height, dispatcher),
    //m_hInstance(hInstance),
    m_label(label)
{
    
    m_ControlType = ControlType::Button;

   // ConsoleManager::getInstance().log(L"[vButton::Constructor] Apelat pentru ID: " + std::wstring(id.begin(), id.end()) +
     //   L", Label: " + label +
       // L", Poziție: (" + std::to_wstring(x) + L", " + std::to_wstring(y) +
        //L"), Dimensiune: " + std::to_wstring(width) + L"x" + std::to_wstring(height));
}


// --- Metoda Create ---
void vButton::create(HWND parent) {
    // Verificări de validitate
    if (!parent) {
        //ConsoleManager::getInstance().log(L"[ERROR] vButton::create: Părintele HWND este nullptr. Butonul nu poate fi creat fără un părinte valid.");
        return;
    }

    //HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);
    if (!m_hInstance) {
        ConsoleManager::getInstance().log(L"[ERROR] vButton::create: Nu s-a putut obține HINSTANCE de la părintele HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(parent)));
        return;
    }

    const int buttonId = getWin32Id(); // Obține ID-ul Win32 unic alocat pentru acest buton.
   // ConsoleManager::getInstance().log(L"[vButton::create] Se creează butonul cu ID intern: '" + std::wstring(m_id.begin(), m_id.end()) +
     //   L"', ID Win32: " + std::to_wstring(buttonId) +
       // L", Poziție: (" + std::to_wstring(m_x) + L", " + std::to_wstring(m_y) +
        //L"), Dimensiune: " + std::to_wstring(m_width) + L"x" + std::to_wstring(m_height));
    //ConsoleManager::getInstance().log(L"[vButton::create] HWND părinte: " + std::to_wstring(reinterpret_cast<uintptr_t>(parent)));

    // Creează controlul WinAPI de tip buton.
    UINT parentDpi = GetDpiForWindow(parent);
    // Apelează metoda de scalare cu DPI-ul obținut
    scale(parentDpi);

    m_handle = CreateWindowExW(
        0,                                   // Stiluri extinse (niciunul în acest caz)
        L"BUTTON",                           // Numele clasei de fereastră WinAPI pentru un buton standard
        m_label.c_str(),                     // Textul (eticheta) butonului
        WS_CHILD |                           // Este un control copil al părintelui
        WS_VISIBLE |                         // Este vizibil la creare
        WS_TABSTOP |                         // Poate fi accesat cu tasta TAB
        //BS_PUSHBUTTON ,                     // Stil standard pentru un buton de apăsare
        BS_OWNERDRAW,
        m_x, m_y, m_width, m_height,         // <<< Acum folosim poziția și dimensiunea stocate
        
        parent,                              // HWND-ul controlului părinte
        (HMENU)(INT_PTR)buttonId,            // ID-ul numeric Win32 al butonului (pentru WM_COMMAND)
        m_hInstance,                           // Handle-ul instanței de la părinte
        this                                 // Pointer către instanța `vButton`. Deși controalele standard nu folosesc lpCreateParams
                                             // pentru a se lega la instanțe C++, Framework-ul tău `vControl` se bazează pe
                                             // această pasare pentru a apela `SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)lParam);`
                                             // în StaticWndProc la WM_NCCREATE/WM_CREATE. Deci, este corect să-l lași aici.
    );

    if (!m_handle) {

        ConsoleManager::getInstance().log(
            L"[ERROR] Creare vButton: Eroare la crearea HWND-ului butonului '"
            + std::wstring(m_id.begin(), m_id.end())
            + L"'. Cod eroare: " + std::to_wstring(GetLastError())
        );
    }
    else {

        scaleFont(GetDpiForWindow(parent));
        if (m_hFont) {
            SendMessage(m_handle, WM_SETFONT, (WPARAM)m_hFont, TRUE);
        }
        // Setăm dimensiunea inițială pentru layout
        GetClientRect(m_handle, &m_originalClientRect);

        //scaleFont(getCurrentDpi());

        // Tooltip doar dacă butonul a fost creat
        if (!m_tooltipText.empty()) {
            TooltipManager::getInstance().addTooltip(m_handle, m_tooltipText);
        //    ConsoleManager::getInstance().log(
          //      L"[vButton::create] TOOLTIP pentru Butonul cu ID '"
            //    + std::wstring(m_id.begin(), m_id.end())
              //  + L"' a fost creat cu succes."
            //);
        }

      //  ConsoleManager::getInstance().log(
        //    L"[vButton::create] Butonul cu ID '"
          //  + std::wstring(m_id.begin(), m_id.end())
            //+ L"' a fost creat cu succes. HWND: "
            //+ std::to_wstring(reinterpret_cast<uintptr_t>(m_handle))
        //);
    }


}

// vButton.cpp (sau fișierul unde este definită implementarea vButton)

LRESULT vButton::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
   // ConsoleManager::getInstance().log(L"[vButton::handleMessage] Mesaj primit de handleMessage al BUTONULUI (ID: " + std::wstring(m_id.begin(), m_id.end()) + L"): " + std::to_wstring(msg));

    switch (msg) {
    
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        // Aceste mesaje pot fi interesante dacă vrei să schimbi vizual butonul
        // când primește/pierde focusul.
      //  ConsoleManager::getInstance().log(L"[vButton::handleMessage] Primit mesaj de focus: " + std::to_wstring(msg));
        break; // Lăsăm DefWindowProc să gestioneze.

        
    // --- ACEASTA ESTE SECȚIUNEA NOUĂ ȘI CRUCIALĂ ---
    case WM_COMMAND: {
        int notificationCode = HIWORD(wParam);
        int controlId = LOWORD(wParam); // ID-ul Win32 al controlului care a generat evenimentul

        if (notificationCode == BN_CLICKED || notificationCode == BN_DBLCLK) {
            this->onClick(); // Aici se declanșează handler-ul înregistrat
            return 0;
        }
        break; // Dacă nu este BN_CLICKED sau nu este pentru butonul nostru, continuăm cu default.
    }
                   
case WM_DRAWITEM: {
    LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT)lParam;
    HDC hdc = lpDIS->hDC;
    RECT rect = lpDIS->rcItem;

    // 1. STĂRI: Verificăm dacă e DEZACTIVAT, Apăsat sau are Focus
    bool isDisabled = (lpDIS->itemState & ODS_DISABLED); // <--- ACEASTA ESTE CHEIA
    bool isPressed = (lpDIS->itemState & ODS_SELECTED) && !isDisabled;
    bool hasFocus = (lpDIS->itemState & ODS_FOCUS) && !isDisabled;

    // 2. FUNDALUL
    // Dacă e disabled, folosim un gri deschis. Dacă e activ, culoarea ta din XML.
    COLORREF bgColor;
    if (isDisabled) {
        bgColor = RGB(240, 240, 240); // Gri foarte deschis pentru fundal disabled
    }
    else {
        bgColor = isPressed ? RGB(220, 220, 220) : m_backgroundColor;
    }

    HBRUSH hBrush = CreateSolidBrush(bgColor);
    FillRect(hdc, &rect, hBrush);
    DeleteObject(hBrush);

    // 3. BORDURA (3D)
    // Dacă e disabled, facem bordura mai deschisă (gri), nu neagră, ca să pară "șters"
    COLORREF borderColor = isDisabled ? RGB(180, 180, 180) : RGB(0, 0, 0);
    HBRUSH hBorderBrush = CreateSolidBrush(borderColor);
    FrameRect(hdc, &rect, hBorderBrush);
    DeleteObject(hBorderBrush);

    // 4. TEXTUL
    SetBkMode(hdc, TRANSPARENT);

    if (isDisabled) {
        SetTextColor(hdc, RGB(160, 160, 160)); // Text gri pentru starea disabled
    }
    else {
        SetTextColor(hdc, m_textColor);
    }

    // Efectul tactil (mutare 1px) se aplică doar dacă e activ și apăsat
    if (isPressed) OffsetRect(&rect, 1, 1);

    //HGDIOBJ oldFont = SelectObject(hdc, m_hFont);
    // Dacă fontul lipsește, folosește fontul implicit de sistem pentru test
    HGDIOBJ oldFont = SelectObject(hdc, m_hFont ? m_hFont : GetStockObject(DEFAULT_GUI_FONT));
    DrawTextW(hdc, m_label.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
    //LOG_DEBUG(L"Buton:" + str_to_wstr(m_id) + L" desenez text:" + m_label);
    // 5. Indicator de FOCUS (doar dacă e activ)
    if (hasFocus && !isPressed && !isDisabled) {
        RECT focusRect = rect;
        InflateRect(&focusRect, -3, -3);
        DrawFocusRect(hdc, &focusRect);
    }

    SelectObject(hdc, oldFont);
    return TRUE;
}

    default:
        break;
    }

    // IMPORTANT: Pasează mesajul către implementarea clasei de bază (vControl),
    // care în cele din urmă apelează DefWindowProc pentru mesaje negestionate explicit.
    // Pentru WM_COMMAND, dacă l-ai gestionat, ai returnat deja 0.
    // Altfel, vControl::handleMessage (și ulterior DefWindowProc) îl va procesa.
    return vControl::handleMessage(hwnd, msg, wParam, lParam);
}
// --- Metoda onClick (suprascrisă) ---
// Această metodă este apelată de logica de dispecerizare a WM_COMMAND
// din vContainer (prin handleChildCommand), nu direct de handleMessage al butonului.
void vButton::onClick() {
   // ConsoleManager::getInstance().log(L"[vButton::onClick] Metoda onClick specifică butonului apelată pentru ID: " + std::wstring(m_id.begin(), m_id.end()));
    // Aici se execută orice logică specifică atunci când butonul este "apăsat".
    // De exemplu, poți schimba starea butonului, lansa un dialog, etc.

    // Apoi, apelăm implementarea de bază pentru a declanșa evenimentul "click" generic
    // prin EventDispatcher, permițând oricărui ascultător să reacționeze.
    vControl::onClick(); // Aceasta va apela `this->dispatch("click");`
}


void vButton::setText(const std::wstring& text) {
    m_label = text; // Actualizăm variabila internă a butonului

    // Apelăm versiunea din baza pentru a face SetWindowTextW efectiv
    vControl::setText(text);
   
        if (m_handle) {
            InvalidateRect(m_handle, NULL, TRUE); // Forțează re-apelarea WM_DRAWITEM
        }
   
    // Opțional: Dacă butonul are dimensiune AUTO, ar trebui să ceri un layout refresh aici
     //this->update(); 
}

// Funcție ajutătoare pentru transformarea numelui în lowercase
static std::wstring toLowerProp(const std::wstring& name) {
    std::wstring lowered = name;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);
    return lowered;
}

bool vButton::setProperty(const std::wstring& name, const vData& value) {
    std::wstring prop = toLowerProp(name);

    // Proprietate specifică doar pentru vButton
    if (prop == L"icon_path" || prop == L"icon") {
        m_iconFilePath = value.toWString();
        m_hasIcon = !m_iconFilePath.empty();
        
        // AICI: Dacă ai deja o metodă care încarcă fizic icoana pe buton în Win32, o apelezi:
        // this->loadIconFromPath(m_iconFilePath);
        
        ConsoleManager::getInstance().log(L"🎨 [vButton] S-a setat icoana: " + m_iconFilePath + L" pe butonul: " + str_to_wstr(m_id));
        return true;
    }

    // Dacă nu este o proprietate de icoană, o trimitem la clasa de bază vControl
    // vControl va rezolva automat: text (label-ul), x, y, width, height, anchor, margin etc.
    return vControl::setProperty(name, value);
}

vData vButton::getProperty(const std::wstring& name) const {
    std::wstring prop = toLowerProp(name);

    if (prop == L"icon_path" || prop == L"icon") {
        return vData(m_iconFilePath);
    }
    if (prop == L"has_icon") {
        return vData(m_hasIcon);
    }

    // Fallback către lanțul de moștenire (vControl)
    return vControl::getProperty(name);
}