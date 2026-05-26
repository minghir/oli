#include "vRichEdit.hpp"
#include "ConsoleManager.hpp"
#include <vector>

// Inițializăm membrul static
HMODULE vRichEdit::s_richEditModule = nullptr;

vRichEdit::vRichEdit(HINSTANCE hInstance, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher)
    : vControl(hInstance, id, x, y, width, height, dispatcher) {

    // Încărcăm DLL-ul necesar pentru Rich Edit 4.1 (sau versiuni mai noi)
    if (!s_richEditModule) {
        s_richEditModule = LoadLibraryW(L"Msftedit.dll");
        if (!s_richEditModule) {
            ConsoleManager::getInstance().log(L"[ERROR] vRichEdit: Nu s-a putut incarca Msftedit.dll!");
        }
    }
}

void vRichEdit::create(HWND parent) {
    if (!parent) return;

    // Stiluri: Adăugăm scrollbar-uri și forțăm comportamentul de editor de cod
    DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_CLIPSIBLINGS;

    if (m_isReadOnly) dwStyle |= ES_READONLY;

    m_handle = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        MSFTEDIT_CLASS, // Clasa definită în richedit.h
        L"",
        dwStyle,
        m_x, m_y, m_width, m_height,
        parent,
        (HMENU)(uintptr_t)getWin32Id(),
        m_hInstance,
        this
    );

    if (m_handle) {
        // Setăm o limită de text mai mare (implicit e mică)
        SendMessage(m_handle, EM_EXLIMITTEXT, 0, (LPARAM)-1);

        // Fontul trebuie să fie Monospaced pentru cod (Courier New sau Consolas)
        HFONT hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            FIXED_PITCH | FF_MODERN, L"Consolas");
        SendMessage(m_handle, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
}

void vRichEdit::setTextColorRange(int start, int end, COLORREF color, bool bold) {
    if (!m_handle) return;

    // 1. Selectăm intervalul
    CHARRANGE cr;
    cr.cpMin = start;
    cr.cpMax = end;
    SendMessage(m_handle, EM_EXSETSEL, 0, (LPARAM)&cr);

    // 2. Aplicăm formatarea
    CHARFORMAT2 cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | (bold ? CFM_BOLD : 0);
    cf.crTextColor = color;
    if (bold) cf.dwEffects |= CFE_BOLD;

    // SCF_SELECTION înseamnă că aplicăm doar pe ce am selectat mai sus
    SendMessage(m_handle, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    // 3. Deselectăm (opțional, mutăm cursorul la final)
    cr.cpMin = end;
    cr.cpMax = end;
    SendMessage(m_handle, EM_EXSETSEL, 0, (LPARAM)&cr);
}

void vRichEdit::freeze() {
    SendMessage(m_handle, WM_SETREDRAW, FALSE, 0);
}

void vRichEdit::unfreeze() {
    SendMessage(m_handle, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_handle, NULL, TRUE);
}

void vRichEdit::setText(const std::wstring& text) {
    if (m_handle) SetWindowTextW(m_handle, text.c_str());
}

std::wstring vRichEdit::getText() const {
    if (!m_handle) return L"";
    int len = GetWindowTextLengthW(m_handle);
    std::vector<wchar_t> buf(len + 1);
    GetWindowTextW(m_handle, buf.data(), len + 1);
    return std::wstring(buf.data());
}

void vRichEdit::setFontSize(int size) {
    if (!m_handle) return;

    // 1. Pregătim structura CHARFORMAT2
    CHARFORMAT2 cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_SIZE | CFM_FACE; // Mască pentru dimensiune și font
    cf.yHeight = size * 20; // 24 puncte * 20 = 480 twips
    
    // Setăm numele fontului
    //wcscpy_s(cf.szFaceName, L"Consolas"); 
	// Setăm numele fontului
#ifdef UNICODE
    wcsncpy(cf.szFaceName, L"Consolas", LF_FACESIZE - 1);
    cf.szFaceName[LF_FACESIZE - 1] = L'\0';
#else
    strncpy(cf.szFaceName, "Consolas", LF_FACESIZE - 1);
    cf.szFaceName[LF_FACESIZE - 1] = '\0';
#endif
    cf.dwEffects = 0; 

    // 2. Aplicăm formatarea:
    // SCF_ALL = Modifică tot textul din editor
    // SCF_DEFAULT = Modifică fontul default pentru tot ce vei scrie ulterior
    SendMessage(m_handle, EM_SETCHARFORMAT, SCF_ALL | SCF_DEFAULT, (LPARAM)&cf);
    
    // 3. Opțional: Pentru a fi siguri, trimitem și WM_SETFONT
    // Deși EM_SETCHARFORMAT este mai important pentru RichEdit
    HFONT hFont = CreateFontW(
        -MulDiv(size, 96, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas"
    );
    SendMessage(m_handle, WM_SETFONT, (WPARAM)hFont, TRUE);
}