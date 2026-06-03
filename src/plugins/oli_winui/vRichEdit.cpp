#include "vRichEdit.hpp"
#include "ConsoleManager.hpp"
#include <vector>
#include <algorithm>

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


vRichEdit::~vRichEdit() {
    if (m_activeFont) DeleteObject(m_activeFont);
    if (s_richEditModule) FreeLibrary(s_richEditModule); // Atenție: s_richEditModule este static
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
		RECT rc;
		GetClientRect(m_handle, &rc);
		SendMessage(m_handle, EM_SETRECT, 0, (LPARAM)&rc);
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
/*
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
*/

void vRichEdit::setFont(const std::wstring& fontName, int baseFontSize, int weight, bool italic, bool underline) {
    // 1. Apelăm clasa de bază pentru ca vControl să salveze atributele (m_fontName, m_baseFontSize, etc.)
    vControl::setFont(fontName, baseFontSize, weight, italic, underline);

    // 2. Acum aplicăm specific pentru RichEdit
    if (!m_handle) return;

    CHARFORMAT2 cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_SIZE | CFM_FACE | CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE;
    cf.yHeight = baseFontSize * 20; // Twips
    wcscpy_s(cf.szFaceName, fontName.c_str());
    
    cf.dwEffects = 0;
    if (weight >= FW_BOLD) cf.dwEffects |= CFE_BOLD;
    if (italic) cf.dwEffects |= CFE_ITALIC;
    if (underline) cf.dwEffects |= CFE_UNDERLINE;

    SendMessage(m_handle, EM_SETCHARFORMAT, SCF_ALL | SCF_DEFAULT, (LPARAM)&cf);
}

void vRichEdit::setFontSize(int baseFontSize) {
    if (!m_handle) return;

    // 1. Calculăm atributele pentru RichEdit
    CHARFORMAT2 cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_SIZE | CFM_FACE;
    cf.yHeight = baseFontSize * 20; 
    wcscpy_s(cf.szFaceName, L"Consolas");
    SendMessage(m_handle, EM_SETCHARFORMAT, SCF_ALL | SCF_DEFAULT, (LPARAM)&cf);

    // 2. Creăm fontul pentru WinAPI (Gutter-ul are nevoie de asta)
    // Calculăm înălțimea în pixeli
    HDC hdc = GetDC(m_handle);
    int pixelHeight = -MulDiv(baseFontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(m_handle, hdc);

    HFONT hFont = CreateFontW(pixelHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    // 3. Update-ul fontului activ și curățarea celui vechi
    if (m_activeFont) DeleteObject(m_activeFont); // Prevenim memory leak
    m_activeFont = hFont;
    SendMessage(m_handle, WM_SETFONT, (WPARAM)m_activeFont, TRUE);
}

void vRichEdit::scaleFont(int newDpi) {
    // 1. Întâi scalăm în baza (vControl va recalcula m_hFont bazat pe DPI)
    vControl::scaleFont(newDpi);

    // 2. Pentru RichEdit, trebuie să aplicăm formatarea recalculată
    int scaledSize = MulDiv(m_baseFontSize, newDpi, 96);
    setFontSize(scaledSize);
}

void vRichEdit::appendText(const std::wstring& text) {
    if (!m_handle) return;
    // 1. Mergem la sfârșitul documentului (-1, -1 selectează sfârșitul)
    SendMessage(m_handle, EM_SETSEL, -1, -1);
    // 2. Inserăm textul
    SendMessage(m_handle, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    // 3. Scroll automat
    SendMessage(m_handle, EM_SCROLLCARET, 0, 0);
}

bool vRichEdit::setProperty(const std::wstring& name, const vData& value) {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    if (prop == L"text") {
        this->setText(value.toWString());
        return true;
    }
    if (prop == L"read_only") {
        this->setReadOnly(value.toBool());
        return true;
    }
    if (prop == L"font_size") {
        this->setFontSize(value.toInt());
        return true;
    }
    return vControl::setProperty(name, value);
}

vData vRichEdit::getProperty(const std::wstring& name) const {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    if (prop == L"text") return vData(this->getText());
    if (prop == L"read_only") return vData(m_isReadOnly);
    
    return vControl::getProperty(name);
}

bool vRichEdit::callMethod(const std::wstring& methodName, const std::vector<vData>& args) {
    std::wstring method = methodName;
    std::transform(method.begin(), method.end(), method.begin(), ::tolower);

    if (method == L"append_text") {
        if (args.empty()) return false;
        this->appendText(args[0].toWString());
        return true;
    }
    if (method == L"set_text") {
        if (args.empty()) return false;
        this->setText(args[0].toWString());
        return true;
    }
    if (method == L"set_read_only") {
        if (args.empty()) return false;
        this->setReadOnly(args[0].toBool());
        return true;
    }

    return vControl::callMethod(methodName, args);
}

void vRichEdit::setReadOnly(bool readOnly) {
    m_isReadOnly = readOnly; // Actualizăm variabila internă
    if (m_handle) {
        SendMessage(m_handle, EM_SETREADONLY, (WPARAM)readOnly, 0);
    }
}
