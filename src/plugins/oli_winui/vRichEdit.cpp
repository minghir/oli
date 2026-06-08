#include "vRichEdit.hpp"
#include "../../ConsoleManager.hpp"
#include <vector>
#include <algorithm>
#include <commctrl.h>
#include <windowsx.h>

HMODULE vRichEdit::s_richEditModule = nullptr;

vRichEdit::vRichEdit(HINSTANCE hInstance, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher)
    : vControl(hInstance, id, x, y, width, height, dispatcher) {
    if (!s_richEditModule) {
        s_richEditModule = LoadLibraryW(L"Msftedit.dll");
    }
}

vRichEdit::~vRichEdit() {
    if (m_activeFont) DeleteObject(m_activeFont);
}

// 🔥 PROCEDURA DE SUBCLASSING CU LOGURI DE DEBUG STRCTE:
LRESULT CALLBACK RichEditTabSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    vRichEdit* pControl = reinterpret_cast<vRichEdit*>(dwRefData);

    switch (uMsg) {
    case WM_GETDLGCODE: {
        return DefSubclassProc(hWnd, uMsg, wParam, lParam) | DLGC_WANTALLKEYS | DLGC_WANTTAB;
    }

    case WM_KEYDOWN: {
        if (wParam == VK_TAB) {
            SendMessageW(hWnd, EM_REPLACESEL, TRUE, (LPARAM)L"\t");
            return 0;
        }
        break;
    }

    case WM_CHAR: {
        if (wParam == VK_TAB) return 0;
        break;
    }

    case WM_SETCURSOR: {
        if (pControl && pControl->m_isResizable) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hWnd, &pt);
            if (pt.y >= 0 && pt.y < pControl->m_resizeMargin) {
                SetCursor(LoadCursor(NULL, IDC_SIZENS));
                return TRUE;
            }
        }
        break;
    }

    case WM_LBUTTONDOWN: {
        if (pControl) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            // Logăm fiecare click stânga în consolă ca să vedem coordonatele reale
            ConsoleManager::getInstance().log(L"[DEBUG C++] Click detectat în RichEdit la Y=" + std::to_wstring(pt.y) + L". m_isResizable=" + std::to_wstring(pControl->m_isResizable));

            if (pControl->m_isResizable) {
                if (pt.y >= 0 && pt.y < pControl->m_resizeMargin) {
                    pControl->m_isResizing = true;
                    SetCapture(hWnd);

                    POINT ptScreen;
                    GetCursorPos(&ptScreen);
                    pControl->m_lastMouseY = ptScreen.y;
                    pControl->m_resizeDelta = 0;

                    ConsoleManager::getInstance().log(L"[DEBUG C++] REDIMENSIONARE DETECTATĂ! Am activat m_isResizing și SetCapture.");
                    return 0;
                }
            }
        }
        break;
    }

    case WM_MOUSEMOVE: {
        if (pControl && pControl->m_isResizing) {
            // Forțăm cursorul de split la fiecare mișcare
            SetCursor(LoadCursor(NULL, IDC_SIZENS));

            POINT ptScreen;
            GetCursorPos(&ptScreen);

            int delta = ptScreen.y - pControl->m_lastMouseY;

            if (delta != 0) {
                pControl->m_resizeDelta = delta;
                pControl->m_lastMouseY = ptScreen.y;

                // 1. Scriptul își face treaba (mărește panoul și rulează refresh pe VSTACK)
                pControl->getEventDispatcher().dispatch("RESIZE_VERTICAL", pControl->getId());

                // 2. 🔥 FIX ANTI-GHOSTING: Ștergem liniile gri lăsate în urmă
                // hWnd este RichEdit -> Părintele este console_panel -> Bunicul este main_panel
                HWND hConsolePanel = GetParent(hWnd);
                if (hConsolePanel) {
                    HWND hMainPanel = GetParent(hConsolePanel);
                    if (hMainPanel) {
                        // InvalidateRect cu TRUE forțează ștergerea completă a pixelilor fantomă
                        InvalidateRect(hMainPanel, NULL, TRUE);

                        // UpdateWindow trimite mesajul de paint instantaneu (fără lag sau buffering)
                        UpdateWindow(hMainPanel);
                    }
                }
            }
            return 0;
        }
        break;
    }

    case WM_LBUTTONUP: {
        if (pControl && pControl->m_isResizing) {
            ConsoleManager::getInstance().log(L"[DEBUG C++] Am dat drumul la click (LBUTTONUP). Opresc redimensionarea.");
            pControl->m_isResizing = false;
            pControl->m_resizeDelta = 0;
            ReleaseCapture();
            return 0;
        }
        break;
    }
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void vRichEdit::create(HWND parent) {
    if (!parent) return;

    DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_CLIPSIBLINGS;

    if (m_isReadOnly) dwStyle |= ES_READONLY;

    m_handle = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        MSFTEDIT_CLASS,
        L"",
        dwStyle,
        m_x, m_y, m_width, m_height,
        parent,
        (HMENU)(uintptr_t)getWin32Id(),
        m_hInstance,
        this
    );

    if (m_handle) {
        SetWindowSubclass(m_handle, RichEditTabSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
        SendMessage(m_handle, EM_EXLIMITTEXT, 0, (LPARAM)-1);

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
    CHARRANGE cr = { start, end };
    SendMessage(m_handle, EM_EXSETSEL, 0, (LPARAM)&cr);

    CHARFORMAT2 cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | (bold ? CFM_BOLD : 0);
    cf.crTextColor = color;
    if (bold) cf.dwEffects |= CFE_BOLD;

    SendMessage(m_handle, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    cr.cpMin = end; cr.cpMax = end;
    SendMessage(m_handle, EM_EXSETSEL, 0, (LPARAM)&cr);
}

void vRichEdit::freeze() { SendMessage(m_handle, WM_SETREDRAW, FALSE, 0); }
void vRichEdit::unfreeze() { SendMessage(m_handle, WM_SETREDRAW, TRUE, 0); InvalidateRect(m_handle, NULL, TRUE); }
void vRichEdit::setText(const std::wstring& text) { if (m_handle) SetWindowTextW(m_handle, text.c_str()); }

std::wstring vRichEdit::getText() const {
    if (!m_handle) return L"";
    int len = GetWindowTextLengthW(m_handle);
    std::vector<wchar_t> buf(len + 1);
    GetWindowTextW(m_handle, buf.data(), len + 1);
    return std::wstring(buf.data());
}

void vRichEdit::setFont(const std::wstring& fontName, int baseFontSize, int weight, bool italic, bool underline) {
    vControl::setFont(fontName, baseFontSize, weight, italic, underline);
    if (!m_handle) return;

    CHARFORMAT2 cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_SIZE | CFM_FACE | CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE;
    cf.yHeight = baseFontSize * 20;
    wcscpy_s(cf.szFaceName, fontName.c_str());

    cf.dwEffects = 0;
    if (weight >= FW_BOLD) cf.dwEffects |= CFE_BOLD;
    if (italic) cf.dwEffects |= CFE_ITALIC;
    if (underline) cf.dwEffects |= CFE_UNDERLINE;

    SendMessage(m_handle, EM_SETCHARFORMAT, SCF_ALL | SCF_DEFAULT, (LPARAM)&cf);
}

void vRichEdit::setFontSize(int baseFontSize) {
    if (!m_handle) return;
    CHARFORMAT2 cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_SIZE | CFM_FACE;
    cf.yHeight = baseFontSize * 20;
    wcscpy_s(cf.szFaceName, L"Consolas");
    SendMessage(m_handle, EM_SETCHARFORMAT, SCF_ALL | SCF_DEFAULT, (LPARAM)&cf);

    HDC hdc = GetDC(m_handle);
    int pixelHeight = -MulDiv(baseFontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(m_handle, hdc);

    HFONT hFont = CreateFontW(pixelHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    if (m_activeFont) DeleteObject(m_activeFont);
    m_activeFont = hFont;
    SendMessage(m_handle, WM_SETFONT, (WPARAM)m_activeFont, TRUE);
}

void vRichEdit::scaleFont(int newDpi) {
    vControl::scaleFont(newDpi);
    int scaledSize = MulDiv(m_baseFontSize, newDpi, 96);
    setFontSize(scaledSize);
}

void vRichEdit::appendText(const std::wstring& text) {
    if (!m_handle) return;
    SendMessage(m_handle, EM_SETSEL, -1, -1);
    SendMessage(m_handle, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    SendMessage(m_handle, EM_SCROLLCARET, 0, 0);
}

bool vRichEdit::setProperty(const std::wstring& name, const vData& value) {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    if (prop == L"text") { this->setText(value.toWString()); return true; }
    if (prop == L"read_only") { this->setReadOnly(value.toBool()); return true; }
    if (prop == L"font_size") { this->setFontSize(value.toInt()); return true; }

    if (prop == L"resizable") {
        ConsoleManager::getInstance().log(L"[DEBUG C++] setProperty('resizable') apeltat cu: " + std::to_wstring(value.toBool()));
        this->setResizable(value.toBool());
        return true;
    }
    if (prop == L"resize_delta") { m_resizeDelta = value.toInt(); return true; }

    return vControl::setProperty(name, value);
}

vData vRichEdit::getProperty(const std::wstring& name) const {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    if (prop == L"text") return vData(this->getText());
    if (prop == L"read_only") return vData(m_isReadOnly);
    if (prop == L"resize_delta") return vData((long long)m_resizeDelta);

    return vControl::getProperty(name);
}

bool vRichEdit::callMethod(const std::wstring& methodName, const std::vector<vData>& args) {
    std::wstring method = methodName;
    std::transform(method.begin(), method.end(), method.begin(), ::tolower);

    if (method == L"append_text") { if (args.empty()) return false; this->appendText(args[0].toWString()); return true; }
    if (method == L"set_text") { if (args.empty()) return false; this->setText(args[0].toWString()); return true; }
    if (method == L"set_read_only") { if (args.empty()) return false; this->setReadOnly(args[0].toBool()); return true; }

    if (method == L"set_resizable") {
        if (args.empty()) return false;
        ConsoleManager::getInstance().log(L"[DEBUG C++] callMethod('set_resizable') apelat cu: " + std::to_wstring(args[0].toBool()));
        this->setResizable(args[0].toBool());
        return true;
    }

    return vControl::callMethod(methodName, args);
}

void vRichEdit::setReadOnly(bool readOnly) {
    m_isReadOnly = readOnly;
    if (m_handle) SendMessage(m_handle, EM_SETREADONLY, (WPARAM)readOnly, 0);
}