#include "vCodeView.hpp"
#include "../../ConsoleManager.hpp" 
#include "stringUtils.hpp"
#include <fstream>
#include <sstream>
#include <algorithm> // Pentru std::replace (folosit în mod standard)
#include <vector>
#include <richedit.h>
#include <iterator> // Pentru std::istreambuf_iterator
#include <filesystem>

#include <commctrl.h>
// Presupunând că această funcție este definită și disponibilă
extern std::wstring utf8_to_wstring(const std::string& str);



// 1. MODIFICĂ PROCEDURA DE SUBCLASS (Adaugă protecția la WM_DESTROY / WM_NCDESTROY)
LRESULT CALLBACK RichEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    vCodeView* codeView = reinterpret_cast<vCodeView*>(dwRefData);

    if (msg == WM_CONTEXTMENU) {
        // Obținem coordonatele mouse-ului
        int xPos = GET_X_LPARAM(lParam);
        int yPos = GET_Y_LPARAM(lParam);

        if (codeView) {
            codeView->showContextMenu(xPos, yPos);
        }
        return 0; // Prevenim afișarea meniului default de RichEdit
    }

    if (msg == WM_DESTROY || msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, RichEditSubclassProc, uIdSubclass);
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

    // Prindem mesajele grafice și de tastatură
    if (msg == WM_VSCROLL || msg == WM_MOUSEWHEEL || msg == WM_PAINT || msg == WM_CHAR) {

        // 1. Permitem controlului să își scrie caracterul pe ecran
        LRESULT res = DefSubclassProc(hwnd, msg, wParam, lParam);

        if (codeView) {
            // Re-desenăm rigla cu numere (codul tău existent)
            codeView->redrawGutter();

            // 2. 🔥 FILTRAREA INTELIGENTĂ: Recolorăm doar la Space sau Enter!
            if (msg == WM_CHAR) {
                wchar_t ch = static_cast<wchar_t>(wParam);

                // 32 = Space (' ')
                // 13 = Enter ('\r' / '\n')
                if (ch == L' ' || ch == 13) {
                    // Invocăm highlight-ul securizat care păstrează cursorul pe poziție
                    codeView->triggerHighlight();
                }
            }
        }
        return res;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// =================================================================
// 🔥 IMPLEMENTEAZĂ DESTRUCTORUL COMPONENTEI vCodeView
// =================================================================
vCodeView::~vCodeView() {
    ConsoleManager::getInstance().log(L"[vCodeView::Destructor] Eliberare latentă în background...");

    // Decuplăm doar hook-ul grafic de pe RichEdit pentru a lăsa controlul curat
    if (m_richEdit && m_richEdit->getHandle()) {
        HWND hEdit = m_richEdit->getHandle();
        if (IsWindow(hEdit)) {
            RemoveWindowSubclass(hEdit, RichEditSubclassProc, 1);
        }
    }
}

bool vCodeView::loadFromFile(const std::wstring& filePath) {
    ConsoleManager::getInstance().log(L"[vCodeView::loadFromFile] Se încarcă fișierul: " + filePath);

    std::ifstream ifs{ std::filesystem::path(filePath), std::ios::binary };
    if (!ifs.is_open()) {
        ConsoleManager::getInstance().log(L"[ERROR] Nu s-a putut deschide fișierul: " + filePath);
        return false;
    }

    std::string utf8Content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    m_currentFilePath = filePath; // Salvăm calea fișierului curent

    if (utf8Content.empty()) {
        m_ignoreChange = true; // 👈 ACTIVĂM PROTECȚIA
        if (m_richEdit) m_richEdit->setText(L"");
        m_ignoreChange = false; // 👈 DEZACTIVĂM
        m_isDirty = false;      // Fișierul e gol, deci e curat
        return true;
    }

    std::wstring fileContent = utf8_to_wstring(utf8Content);
    fileContent.erase(std::remove(fileContent.begin(), fileContent.end(), L'\r'), fileContent.end());

    std::wstring normalizedContent;
    normalizedContent.reserve(fileContent.size() + 100);
    for (wchar_t c : fileContent) {
        if (c == L'\n') normalizedContent += L'\r';
        normalizedContent += c;
    }

    if (m_richEdit) {
        m_ignoreChange = true; // 👈 ACTIVĂM PROTECȚIA
        m_richEdit->setText(normalizedContent);
        m_ignoreChange = false; // 👈 DEZACTIVĂM
    }

    m_isDirty = false; // 🔥 Fișierul proaspăt încărcat este curat!
    return true;
}

void vCodeView::setReadOnly(bool readOnly) {
    if (m_richEdit && m_richEdit->getHandle()) {
        SendMessage(m_richEdit->getHandle(), EM_SETREADONLY, (WPARAM)readOnly, 0);
        std::wstring status = readOnly ? L"READ-ONLY" : L"EDITABLE";
        ConsoleManager::getInstance().log(L"[vCodeView::setReadOnly] ID: " + utf8_to_wstring(m_id) + L" setat la: " + status);
    }
}

void vCodeView::setFontSize(int size) {
    m_fontSize = size; // Actualizează baza pentru scalare

    // 1. Trimite comanda către controlul RICHEDIT (cel care desenează textul)
    if (m_richEdit) {
        m_ignoreChange = true; // 🔥 FIX: Protejăm împotriva EN_CHANGE fals generat de Win32 la schimbarea fontului
        m_richEdit->setFontSize(size);
        m_ignoreChange = false; // 🔥 Dezactivăm protecția
    }

    InvalidateRect(m_richEdit->getHandle(), NULL, TRUE);
    UpdateWindow(m_richEdit->getHandle());

    // Redesenează Gutter-ul acum că avem înălțimea nouă
    this->redrawGutter();

    if (m_richEdit) {
        m_ignoreChange = true; // 🔥 FIX: Protejăm și highlight-ul provocat de font
        m_lexer.highlight(m_richEdit);
        m_ignoreChange = false;
    }

    applyLayout();
}

void vCodeView::scaleFont(int newDpi) {
    vPanel::scaleFont(getCurrentDpi());
    int scaledSize = MulDiv(m_fontSize, newDpi, 96);
}

LRESULT vCodeView::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        RECT richEditArea = { m_gutterWidth, rcClient.top, rcClient.right, rcClient.bottom };
        HBRUSH hBgBrush = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
        FillRect(hdc, &richEditArea, hBgBrush);
        DeleteObject(hBgBrush);

        drawLineNumbers(hdc);
        return 1;
    }

    case WM_PAINT: {
        LRESULT res = vPanel::handleMessage(hwnd, msg, wParam, lParam);
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        drawLineNumbers(hdc);
        EndPaint(hwnd, &ps);
        return res;
    }

                 // 🔥 DEBUG BINDING AICI: Inspectăm mesajele transmise de Windows
    case WM_COMMAND: {
        WORD notificationCode = HIWORD(wParam);
        HWND hwndControl = (HWND)lParam;

        if (m_richEdit && hwndControl == m_richEdit->getHandle()) {
            // Logăm codul de notificare primit de la RichEdit (vrem să vedem 768, adică EN_CHANGE)
            ConsoleManager::getInstance().log(L"[DEBUG C++] WM_COMMAND primit de la RichEdit. Code notificat: " + std::to_wstring(notificationCode));

            if (notificationCode == EN_CHANGE) {
                // Logăm starea flag-urilor interne de protecție
                ConsoleManager::getInstance().log(L"[DEBUG C++] EN_CHANGE detectat! m_ignoreChange = " + std::to_wstring(m_ignoreChange) + L", m_isDirty = " + std::to_wstring(m_isDirty));

                this->redrawGutter();

                if (!m_ignoreChange && !m_isDirty) {
                    m_isDirty = true;

                    ConsoleManager::getInstance().log(L"[DEBUG C++] CONDITIE REUSITA! Lansez evenimentul 'modified' pentru ID-ul: " + utf8_to_wstring(m_id));

                    // Declanșăm evenimentul "modified" prin dispatcher
                    m_dispatcher.dispatch("modified", m_id);
                }
            }
        }
        break;
    }

    case WM_NOTIFY: {
        LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>(lParam);
        if (m_richEdit && nmhdr->hwndFrom == m_richEdit->getHandle()) {
            if (nmhdr->code == EN_VSCROLL) {
                this->redrawGutter();
            }
        }
        break;
    }
    }
    return vPanel::handleMessage(hwnd, msg, wParam, lParam);
}

void vCodeView::drawLineNumbers(HDC hdc) {
    if (!m_richEdit || !m_richEdit->getHandle()) return;

    HWND hEdit = m_richEdit->getHandle();
    HFONT hMyFont = m_richEdit->getActiveFont();
    if (!hMyFont) hMyFont = (HFONT)SendMessage(hEdit, WM_GETFONT, 0, 0);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hMyFont);

    RECT gutterRect = { 0, 0, m_gutterWidth, m_height };
    FillRect(hdc, &gutterRect, (HBRUSH)GetStockObject(WHITE_BRUSH));

    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, m_gutterWidth - 1, 0, NULL);
    LineTo(hdc, m_gutterWidth - 1, m_height);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    SetTextColor(hdc, RGB(140, 140, 140));
    SetBkMode(hdc, TRANSPARENT);

    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;

    RECT rcEdit;
    SendMessage(hEdit, EM_GETRECT, 0, (LPARAM)&rcEdit);
    int firstLine = (int)SendMessage(hEdit, EM_GETFIRSTVISIBLELINE, 0, 0);
    int totalLines = (int)SendMessage(hEdit, EM_GETLINECOUNT, 0, 0);

    int currentLine = firstLine;
    while (currentLine < totalLines) {
        int charIndex = (int)SendMessage(hEdit, EM_LINEINDEX, currentLine, 0);
        if (charIndex == -1) break;

        POINT pt;
        SendMessage(hEdit, EM_POSFROMCHAR, (WPARAM)&pt, charIndex);

        if (pt.y > rcEdit.bottom) break;

        std::wstring lineNumStr = std::to_wstring(currentLine + 1);

        RECT numRect;
        numRect.left = 0;
        numRect.right = m_gutterWidth - 6;
        numRect.top = pt.y;
        numRect.bottom = pt.y + lineHeight;

        DrawTextW(hdc, lineNumStr.c_str(), -1, &numRect, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);

        currentLine++;
    }

    SelectObject(hdc, hOldFont);
}

void vCodeView::redrawGutter() {
    int neededWidth = calculateGutterWidth();
    if (neededWidth != m_gutterWidth) {
        this->applyLayout();
        return;
    }

    HWND hwndPanel = this->getHandle();
    if (hwndPanel && IsWindow(hwndPanel)) {
        HDC hdcPanel = GetDC(hwndPanel);
        drawLineNumbers(hdcPanel);
        ReleaseDC(hwndPanel, hdcPanel);
    }
}

void vCodeView::create(HWND parent) {
    vPanel::create(parent);
    initContextMenu();
    m_gutterWidth = calculateGutterWidth();

    auto rich = std::make_unique<vRichEdit>(m_hInstance, m_id + "_edit", m_gutterWidth, 0, m_width - m_gutterWidth, m_height, getEventDispatcher());
    m_richEdit = rich.get();

    m_richEdit->setMargins(5, 0, 0, 0);
    m_richEdit->setHeightMode(SizeMode::FILL);
    m_richEdit->setWidthMode(SizeMode::FILL);
    m_richEdit->setFontSize(m_fontSize);

    this->addChild(m_id + "_edit", std::move(rich));

    if (m_richEdit->getHandle()) {
        HWND hEdit = m_richEdit->getHandle();
        SetWindowSubclass(hEdit, RichEditSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));

        // 🔥 DEBUG EVENT MASK: Forțăm înregistrarea masicilor și verificăm rezultatul
        LRESULT oldMask = SendMessage(hEdit, EM_SETEVENTMASK, 0, ENM_SCROLL | ENM_CHANGE);
        ConsoleManager::getInstance().log(L"[DEBUG C++] Masca de evenimente aplicata pe RichEdit. Masca precedenta: " + std::to_wstring(oldMask));
    }

    this->applyLayout();
    InvalidateRect(this->getHandle(), NULL, TRUE);
}

void vCodeView::applyLayout() {
    m_gutterWidth = calculateGutterWidth();
    if (m_richEdit) {
        m_richEdit->moveAndResize(m_gutterWidth, 0, m_width - m_gutterWidth, m_height);
    }

    HWND hwndPanel = this->getHandle();
    if (hwndPanel && IsWindow(hwndPanel)) {
        HDC hdcPanel = GetDC(hwndPanel);
        drawLineNumbers(hdcPanel);
        ReleaseDC(hwndPanel, hdcPanel);
    }
}

static std::wstring toLowerPropCodeView(const std::wstring& name) {
    std::wstring lowered = name;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);
    return lowered;
}

bool vCodeView::setProperty(const std::wstring& name, const vData& value) {
    std::wstring prop = toLowerPropCodeView(name);

    if (prop == L"syntax_path" || prop == L"syntax") {
        this->setSyntaxPath(value.toWString());
        return true;
    }

    if (prop == L"file_path" || prop == L"current_file") {
        m_currentFilePath = value.toWString();
        return true;
    }

    if (prop == L"modified" || prop == L"is_dirty") {
        m_isDirty = value.toBool();
        return true;
    }

    return vPanel::setProperty(name, value);
}

vData vCodeView::getProperty(const std::wstring& name) const {
    std::wstring prop = toLowerPropCodeView(name);

    if (prop == L"syntax_path" || prop == L"syntax") {
        return vData(this->getSyntaxPath());
    }

    if (prop == L"file_path" || prop == L"current_file") {
        return vData(m_currentFilePath);
    }

    if (prop == L"modified" || prop == L"is_dirty") {
        return vData(m_isDirty);
    }

    return vPanel::getProperty(name);
}

int vCodeView::calculateGutterWidth() {
    if (!m_richEdit || !m_richEdit->getHandle()) return 50;

    int totalLines = (int)SendMessage(m_richEdit->getHandle(), EM_GETLINECOUNT, 0, 0);
    if (totalLines < 100) totalLines = 100;
    std::wstring maxLineStr = std::to_wstring(totalLines);

    HDC hdc = GetDC(m_richEdit->getHandle());
    HFONT hFont = m_richEdit->getActiveFont();
    if (!hFont) hFont = (HFONT)SendMessage(m_richEdit->getHandle(), WM_GETFONT, 0, 0);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    SIZE size;
    GetTextExtentPoint32W(hdc, maxLineStr.c_str(), (int)maxLineStr.length(), &size);

    SelectObject(hdc, hOldFont);
    ReleaseDC(m_richEdit->getHandle(), hdc);

    return size.cx + 10;
}

void vCodeView::setText(const std::wstring& text) {
    if (m_richEdit) {
        m_ignoreChange = true;

        SendMessage(m_richEdit->getHandle(), WM_SETREDRAW, FALSE, 0);
        m_richEdit->setText(text);
        m_lexer.highlight(m_richEdit);
        SendMessage(m_richEdit->getHandle(), WM_SETREDRAW, TRUE, 0);

        m_ignoreChange = false;
        m_isDirty = false;

        this->applyLayout();
        InvalidateRect(m_richEdit->getHandle(), NULL, TRUE);
    }
}

void vCodeView::setSyntaxPath(const std::wstring& syntaxPath) {
    m_syntaxPath = syntaxPath;
    std::string pathAnsi(syntaxPath.begin(), syntaxPath.end());
    m_lexer.loadSyntaxes(pathAnsi);

    if (syntaxPath.find(L"oli.xml") != std::wstring::npos) {
        m_lexer.setLanguageByFile(L"dummy.oli");
    }
    else if (syntaxPath.find(L"cpp.xml") != std::wstring::npos) {
        m_lexer.setLanguageByFile(L"dummy.cpp");
    }
    else {
        m_lexer.setLanguageByFile(syntaxPath);
    }

    if (m_richEdit) {
        m_ignoreChange = true; // 🔥 FIX: Împiedicăm highlight-ul inițial de la Open File să trimită un EN_CHANGE fals

        SendMessage(m_richEdit->getHandle(), WM_SETREDRAW, FALSE, 0);
        m_lexer.highlight(m_richEdit);
        SendMessage(m_richEdit->getHandle(), WM_SETREDRAW, TRUE, 0);

        m_ignoreChange = false; // 🔥 Dezactivăm
        InvalidateRect(m_richEdit->getHandle(), NULL, TRUE);
    }
}

void vCodeView::moveAndResize(int x, int y, int width, int height) {
    vPanel::moveAndResize(x, y, width, height);
    this->applyLayout();
}

void vCodeView::triggerHighlight() {
    if (!m_richEdit || !m_richEdit->getHandle()) return;
    HWND hEdit = m_richEdit->getHandle();

    ::SendMessageW(hEdit, WM_SETREDRAW, FALSE, 0);

    POINT scrollPos = { 0, 0 };
    ::SendMessageW(hEdit, EM_GETSCROLLPOS, 0, (LPARAM)&scrollPos);

    CHARRANGE cr;
    ::SendMessageW(hEdit, EM_EXGETSEL, 0, (LPARAM)&cr);

    m_lexer.highlight(m_richEdit);

    ::SendMessageW(hEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
    ::SendMessageW(hEdit, EM_SETSCROLLPOS, 0, (LPARAM)&scrollPos);

    ::SendMessageW(hEdit, WM_SETREDRAW, TRUE, 0);
    ::InvalidateRect(hEdit, NULL, TRUE);
}

void vCodeView::initContextMenu() {
    std::string uniqueMenuId = "ctx_menu_" + m_id;
    m_contextMenu = std::make_unique<vPopupMenu>(uniqueMenuId, m_dispatcher);

    m_contextMenu->addItem("ctx_cut_" + m_id, L"Cut");
    m_contextMenu->addItem("ctx_copy_" + m_id, L"Copy");
    m_contextMenu->addItem("ctx_paste_" + m_id, L"Paste");
    m_contextMenu->addSeparator("sep_" + m_id);
    m_contextMenu->addItem("ctx_delete_" + m_id, L"Delete");
    m_contextMenu->addSeparator("sep1_" + m_id);
    m_contextMenu->addItem("ctx_comment_" + m_id, L"Comment Line");
    m_contextMenu->addItem("ctx_uncomment_" + m_id, L"Uncomment Line");
    m_contextMenu->create(m_handle);
}

void vCodeView::showContextMenu(int x, int y) {
    if (!m_contextMenu) return;

    int cmd = m_contextMenu->display(m_handle, x, y);
    if (cmd == 0) return;

    HWND hEdit = m_richEdit->getHandle();
    SetFocus(hEdit);

    int idCut = m_contextMenu->getMenuItemId("ctx_cut_" + m_id);
    int idCopy = m_contextMenu->getMenuItemId("ctx_copy_" + m_id);
    int idPaste = m_contextMenu->getMenuItemId("ctx_paste_" + m_id);
    int idDelete = m_contextMenu->getMenuItemId("ctx_delete_" + m_id);
    int idComment = m_contextMenu->getMenuItemId("ctx_comment_" + m_id);
    int idUncomment = m_contextMenu->getMenuItemId("ctx_uncomment_" + m_id);

    if (cmd == idCut) {
        SendMessage(hEdit, WM_CUT, 0, 0);
    }
    else if (cmd == idCopy) {
        SendMessage(hEdit, WM_COPY, 0, 0);
    }
    else if (cmd == idPaste) {
        SendMessage(hEdit, WM_PASTE, 0, 0);
    }
    else if (cmd == idDelete) {
        SendMessage(hEdit, WM_CLEAR, 0, 0);
    }
    else if (cmd == idComment) {
        this->commentLines(true);
    }
    else if (cmd == idUncomment) {
        this->commentLines(false);
    }
}

void vCodeView::commentLines(bool comment) {
    if (!m_richEdit || !m_richEdit->getHandle()) return;
    HWND hEdit = m_richEdit->getHandle();

    CHARRANGE cr;
    SendMessage(hEdit, EM_EXGETSEL, 0, (LPARAM)&cr);

    int len = cr.cpMax - cr.cpMin;
    if (len <= 0) return;

    std::vector<wchar_t> buffer(len + 1);
    TEXTRANGEW tr;
    tr.chrg = cr;
    tr.lpstrText = buffer.data();
    SendMessageW(hEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    std::wstring text(buffer.data());

    std::vector<std::wstring> lines;
    std::wstringstream ss(text);
    std::wstring line;
    while (std::getline(ss, line, L'\n')) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        lines.push_back(line);
    }

    std::wstring newText;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (comment) {
            if (lines[i].find(L"#") != 0) {
                newText += L"# " + lines[i];
            }
            else {
                newText += lines[i];
            }
        }
        else {
            if (lines[i].find(L"# ") == 0) {
                newText += lines[i].substr(2);
            }
            else if (lines[i].find(L"#") == 0) {
                newText += lines[i].substr(1);
            }
            else {
                newText += lines[i];
            }
        }

        if (i < lines.size() - 1) newText += L"\r\n";
    }

    SendMessageW(hEdit, EM_REPLACESEL, TRUE, (LPARAM)newText.c_str());
    m_lexer.highlight(m_richEdit);
}


/*
#include "vCodeView.hpp"
#include "../../ConsoleManager.hpp" 
#include "stringUtils.hpp"
#include <fstream>
#include <sstream>
#include <algorithm> // Pentru std::replace (folosit în mod standard)
#include <vector>
#include <richedit.h>
#include <iterator> // Pentru std::istreambuf_iterator
#include <filesystem>

#include <commctrl.h>
// Presupunând că această funcție este definită și disponibilă
extern std::wstring utf8_to_wstring(const std::string& str);



// 1. MODIFICĂ PROCEDURA DE SUBCLASS (Adaugă protecția la WM_DESTROY / WM_NCDESTROY)
LRESULT CALLBACK RichEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    vCodeView* codeView = reinterpret_cast<vCodeView*>(dwRefData);

	if (msg == WM_CONTEXTMENU) {
        // Obținem coordonatele mouse-ului
        int xPos = GET_X_LPARAM(lParam);
        int yPos = GET_Y_LPARAM(lParam);

        if (codeView) {
            codeView->showContextMenu(xPos, yPos);
        }
        return 0; // Prevenim afișarea meniului default de RichEdit
    }

    if (msg == WM_DESTROY || msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, RichEditSubclassProc, uIdSubclass);
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

    // Prindem mesajele grafice și de tastatură
    if (msg == WM_VSCROLL || msg == WM_MOUSEWHEEL || msg == WM_PAINT || msg == WM_CHAR) {

        // 1. Permitem controlului să își scrie caracterul pe ecran
        LRESULT res = DefSubclassProc(hwnd, msg, wParam, lParam);

        if (codeView) {
            // Re-desenăm rigla cu numere (codul tău existent)
            codeView->redrawGutter();

            // 2. 🔥 FILTRAREA INTELIGENTĂ: Recolorăm doar la Space sau Enter!
            if (msg == WM_CHAR) {
                wchar_t ch = static_cast<wchar_t>(wParam);

                // 32 = Space (' ')
                // 13 = Enter ('\r' / '\n')
                if (ch == L' ' || ch == 13) {
                    // Invocăm highlight-ul securizat care păstrează cursorul pe poziție
                    codeView->triggerHighlight();
                }
            }
        }
        return res;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// =================================================================
// 🔥 IMPLEMENTEAZĂ DESTRUCTORUL COMPONENTEI vCodeView
// =================================================================
vCodeView::~vCodeView() {
    ConsoleManager::getInstance().log(L"[vCodeView::Destructor] Eliberare latentă în background...");

    // Decuplăm doar hook-ul grafic de pe RichEdit pentru a lăsa controlul curat
    if (m_richEdit && m_richEdit->getHandle()) {
        HWND hEdit = m_richEdit->getHandle();
        if (IsWindow(hEdit)) {
            RemoveWindowSubclass(hEdit, RichEditSubclassProc, 1);
        }
    }
}

// Presupunând că str_to_wstr este disponibil pentru logare
// extern std::wstring str_to_wstr(const std::string& s); 

bool vCodeView::loadFromFile(const std::wstring& filePath) {
    ConsoleManager::getInstance().log(L"[vCodeView::loadFromFile] Se încarcă fișierul: " + filePath);

    std::ifstream ifs{ std::filesystem::path(filePath), std::ios::binary };
    if (!ifs.is_open()) {
        ConsoleManager::getInstance().log(L"[ERROR] Nu s-a putut deschide fișierul: " + filePath);
        return false;
    }

    std::string utf8Content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    m_currentFilePath = filePath; // Salvăm calea fișierului curent

    if (utf8Content.empty()) {
        m_ignoreChange = true; // 👈 ACTIVĂM PROTECȚIA
        if (m_richEdit) m_richEdit->setText(L"");
        m_ignoreChange = false; // 👈 DEZACTIVĂM
        m_isDirty = false;      // Fișierul e gol, deci e curat
        return true;
    }

    std::wstring fileContent = utf8_to_wstring(utf8Content);
    fileContent.erase(std::remove(fileContent.begin(), fileContent.end(), L'\r'), fileContent.end());

    std::wstring normalizedContent;
    normalizedContent.reserve(fileContent.size() + 100);
    for (wchar_t c : fileContent) {
        if (c == L'\n') normalizedContent += L'\r';
        normalizedContent += c;
    }

    if (m_richEdit) {
        m_ignoreChange = true; // 👈 ACTIVĂM PROTECȚIA
        m_richEdit->setText(normalizedContent);
        m_ignoreChange = false; // 👈 DEZACTIVĂM
    }

    m_isDirty = false; // 🔥 Fișierul proaspăt încărcat este curat!
    return true;
}

// Metoda pentru a seta modul Read-Only (utilă pentru vizualizare cod)
void vCodeView::setReadOnly(bool readOnly) {
    // Verificăm dacă avem editorul creat și dacă el are un handle valid
    if (m_richEdit && m_richEdit->getHandle()) {
        SendMessage(m_richEdit->getHandle(), EM_SETREADONLY, (WPARAM)readOnly, 0);

        std::wstring status = readOnly ? L"READ-ONLY" : L"EDITABLE";
        ConsoleManager::getInstance().log(L"[vCodeView::setReadOnly] ID: " + str_to_wstr(m_id) + L" setat la: " + status);
    }
}


void vCodeView::setFontSize(int size) {
    m_fontSize = size; // Actualizează baza pentru scalare

    // 1. Trimite comanda către controlul RICHEDIT (cel care desenează textul)
    if (m_richEdit) {
        m_richEdit->setFontSize(size); 
    }
    
	InvalidateRect(m_richEdit->getHandle(), NULL, TRUE);
    UpdateWindow(m_richEdit->getHandle());
    
    // Redesenează Gutter-ul acum că avem înălțimea nouă
    this->redrawGutter();
    m_lexer.highlight(m_richEdit);
	applyLayout();
}


void vCodeView::scaleFont(int newDpi) {
    // 1. Apelăm scalarea de bază (pentru fontul containerului/gutter-ului dacă e cazul)
    vPanel::scaleFont(getCurrentDpi() );

    // 2. Calculăm dimensiunea corectă în puncte (Puncte = pixeli * 72 / DPI)
    // Sau, dacă m_fontSize este în "puncte", trebuie să-l scalăm în funcție de DPI
    int scaledSize = MulDiv(m_fontSize, newDpi, 96);

    // 3. Trimitem către RichEdit
	//setFontSize(getCurrentDpi() );
		
}

LRESULT vCodeView::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        // Interceptăm momentul în care Windows vrea să șteargă fundalul
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        RECT richEditArea = { m_gutterWidth, rcClient.top, rcClient.right, rcClient.bottom };
        HBRUSH hBgBrush = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
        FillRect(hdc, &richEditArea, hBgBrush);
        DeleteObject(hBgBrush);

        drawLineNumbers(hdc);
        return 1;
    }

    case WM_PAINT: {
        LRESULT res = vPanel::handleMessage(hwnd, msg, wParam, lParam);
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        drawLineNumbers(hdc);
        EndPaint(hwnd, &ps);
        return res;
    }

                 // 🔥 FIXUL SALVATOR 1: Adăugăm WM_COMMAND special pentru EN_CHANGE
    case WM_COMMAND: {
        WORD notificationCode = HIWORD(wParam);
        HWND hwndControl = (HWND)lParam;

        if (m_richEdit && hwndControl == m_richEdit->getHandle()) {
            if (notificationCode == EN_CHANGE) {
                this->redrawGutter();

                // Dacă nu suntem în modul de încărcare/resetare și fișierul nu era deja marcat ca modificat
                if (!m_ignoreChange && !m_isDirty) {
                    m_isDirty = true;

                    // Declanșăm evenimentul "modified" prin dispatcher
                    m_dispatcher.dispatch("modified", m_id);
                }
            }
        }
        break;
    }

                   // Actualizăm WM_NOTIFY să păstreze DOAR mesajele de scroll
    case WM_NOTIFY: {
        LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>(lParam);
        if (m_richEdit && nmhdr->hwndFrom == m_richEdit->getHandle()) {
            if (nmhdr->code == EN_VSCROLL) {
                this->redrawGutter();
            }
        }
        break;
    }
    }
    return vPanel::handleMessage(hwnd, msg, wParam, lParam);
}


void vCodeView::drawLineNumbers(HDC hdc) {
    if (!m_richEdit || !m_richEdit->getHandle()) return;

    HWND hEdit = m_richEdit->getHandle();
    HFONT hMyFont = m_richEdit->getActiveFont();
    if (!hMyFont) hMyFont = (HFONT)SendMessage(hEdit, WM_GETFONT, 0, 0);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hMyFont);

    // 1. Fundalul Gutter-ului (Exact pe lățimea m_gutterWidth)
    RECT gutterRect = { 0, 0, m_gutterWidth, m_height };
    FillRect(hdc, &gutterRect, (HBRUSH)GetStockObject(WHITE_BRUSH));

    // 2. Linia de demarcație neagră (exact pe marginea din dreapta)
    // Desenăm o linie verticală la m_gutterWidth - 1
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0)); 
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, m_gutterWidth - 1, 0, NULL);
    LineTo(hdc, m_gutterWidth - 1, m_height);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    SetTextColor(hdc, RGB(140, 140, 140));
    SetBkMode(hdc, TRANSPARENT);

    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;

    RECT rcEdit;
    SendMessage(hEdit, EM_GETRECT, 0, (LPARAM)&rcEdit);
    int firstLine = (int)SendMessage(hEdit, EM_GETFIRSTVISIBLELINE, 0, 0);
    int totalLines = (int)SendMessage(hEdit, EM_GETLINECOUNT, 0, 0);

    int currentLine = firstLine;
    while (currentLine < totalLines) {
        int charIndex = (int)SendMessage(hEdit, EM_LINEINDEX, currentLine, 0);
        if (charIndex == -1) break;

        POINT pt;
        SendMessage(hEdit, EM_POSFROMCHAR, (WPARAM)&pt, charIndex);

        if (pt.y > rcEdit.bottom) break;

        std::wstring lineNumStr = std::to_wstring(currentLine + 1);
        
        // --- DREPTUNGHIUL DE DESENARE ---
        // Right: m_gutterWidth - 6 (lăsăm 6px distanță față de linia neagră)
        RECT numRect;
        numRect.left = 0;              
        numRect.right = m_gutterWidth - 6 ; 
        numRect.top = pt.y;            
        numRect.bottom = pt.y + lineHeight;

        DrawTextW(hdc, lineNumStr.c_str(), -1, &numRect, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);

        currentLine++;
    }

    SelectObject(hdc, hOldFont);
}

void vCodeView::redrawGutter() {
    int neededWidth = calculateGutterWidth();

    // Dacă numărul de cifre s-a schimbat (ex: s-a trecut de la linia 99 la 100), re-aranjăm totul
    if (neededWidth != m_gutterWidth) {
        this->applyLayout();
        return;
    }

    // Altfel doar redesenăm cifrele (la scroll, taste, etc.)
    HWND hwndPanel = this->getHandle();
    if (hwndPanel && IsWindow(hwndPanel)) {
        HDC hdcPanel = GetDC(hwndPanel);
        drawLineNumbers(hdcPanel);
        ReleaseDC(hwndPanel, hdcPanel);
    }
}


void vCodeView::create(HWND parent) {
    vPanel::create(parent);

    // ❌ ELIMINĂM LINIA: setLayoutStrategy(std::make_unique<AnchorLayout>());
    // Nu punem nicio strategie, ne vom ocupa noi manual de layout în applyLayout()!
	initContextMenu();
    m_gutterWidth = calculateGutterWidth();

    // Creăm RichEdit-ul decalat inițial corect
    auto rich = std::make_unique<vRichEdit>(m_hInstance, m_id + "_edit", m_gutterWidth, 0, m_width - m_gutterWidth, m_height, getEventDispatcher());
    m_richEdit = rich.get();

    m_richEdit->setMargins(5, 0, 0, 0);
    m_richEdit->setHeightMode(SizeMode::FILL);
    m_richEdit->setWidthMode(SizeMode::FILL);
    m_richEdit->setFontSize(m_fontSize);

    this->addChild(m_id + "_edit", std::move(rich));

    if (m_richEdit->getHandle()) {
        HWND hEdit = m_richEdit->getHandle();
        SetWindowSubclass(hEdit, RichEditSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
        SendMessage(hEdit, EM_SETEVENTMASK, 0, ENM_SCROLL | ENM_CHANGE);
    }

    // Apelăm noul nostru layout custom local
    this->applyLayout();

    InvalidateRect(this->getHandle(), NULL, TRUE);
}

void vCodeView::applyLayout() {
    // Îi dezactivăm complet comportamentul de container standard.
    // Calculăm dinamic lățimea riglei pe baza textului actual
    m_gutterWidth = calculateGutterWidth();

    if (m_richEdit) {
        // Forțăm RichEdit-ul să ocupe EXACT spațiul rămas, la milimetru, fără ajutorul AnchorLayout!
        m_richEdit->moveAndResize(m_gutterWidth, 0, m_width - m_gutterWidth, m_height);
    }

    // Forțăm redesenarea riglei cu cifre
    HWND hwndPanel = this->getHandle();
    if (hwndPanel && IsWindow(hwndPanel)) {
        HDC hdcPanel = GetDC(hwndPanel);
        drawLineNumbers(hdcPanel);
        ReleaseDC(hwndPanel, hdcPanel);
    }
}

// Funcție ajutătoare locală pentru transformarea numelui în lowercase
static std::wstring toLowerPropCodeView(const std::wstring& name) {
    std::wstring lowered = name;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);
    return lowered;
}

bool vCodeView::setProperty(const std::wstring& name, const vData& value) {
    std::wstring prop = toLowerPropCodeView(name);

    // Proprietate unică, specifică DOAR pentru vCodeView
    if (prop == L"syntax_path" || prop == L"syntax") {
        this->setSyntaxPath(value.toWString());
        return true;
    }

    if (prop == L"file_path" || prop == L"current_file") {
        m_currentFilePath = value.toWString();
        return true;
    }

    if (prop == L"modified" || prop == L"is_dirty") {
        m_isDirty = value.toBool();
        return true;
    }
    
    // Cascadrăm proprietatea către clasa părinte vPanel
    return vPanel::setProperty(name, value);
}

vData vCodeView::getProperty(const std::wstring& name) const {
    std::wstring prop = toLowerPropCodeView(name);

    if (prop == L"syntax_path" || prop == L"syntax") {
        return vData(this->getSyntaxPath());
    }
    
    if (prop == L"file_path" || prop == L"current_file") {
        return vData(m_currentFilePath);
    }

    if (prop == L"modified" || prop == L"is_dirty") {
        return vData(m_isDirty);
    }

    // Dacă nu este o proprietate unică a editorului de cod, lăsăm ierarhia superioară să o caute
    return vPanel::getProperty(name);
}

int vCodeView::calculateGutterWidth() {
    if (!m_richEdit || !m_richEdit->getHandle()) return 50;

    // Aflăm numărul maxim de linii pentru a ști câte cifre trebuie să încapă
    int totalLines = (int)SendMessage(m_richEdit->getHandle(), EM_GETLINECOUNT, 0, 0);
    if (totalLines < 100) totalLines = 100; // Asigură minim 3 cifre spațiu
    std::wstring maxLineStr = std::to_wstring(totalLines);
    
    // FOLOSEȘTE FONTUL ACTIV (la fel ca în drawLineNumbers)
    HDC hdc = GetDC(m_richEdit->getHandle());
    HFONT hFont = m_richEdit->getActiveFont();
    if (!hFont) hFont = (HFONT)SendMessage(m_richEdit->getHandle(), WM_GETFONT, 0, 0);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    SIZE size;
    GetTextExtentPoint32W(hdc, maxLineStr.c_str(), (int)maxLineStr.length(), &size);

    SelectObject(hdc, hOldFont);
    ReleaseDC(m_richEdit->getHandle(), hdc);

    // Padding-ul trebuie să fie exact cel din drawLineNumbers (8px dreapta + puțin stânga)
    return size.cx + 10; 
}

void vCodeView::setText(const std::wstring& text) {
    if (m_richEdit) {
        m_ignoreChange = true; // 👈 ACTIVĂM PROTECȚIA

        SendMessage(m_richEdit->getHandle(), WM_SETREDRAW, FALSE, 0);
        m_richEdit->setText(text);
        m_lexer.highlight(m_richEdit);
        SendMessage(m_richEdit->getHandle(), WM_SETREDRAW, TRUE, 0);

        m_ignoreChange = false; // 👈 DEZACTIVĂM
        m_isDirty = false;      // 🔥 Textul setat programatic înseamnă resetare de stare

        this->applyLayout();
        InvalidateRect(m_richEdit->getHandle(), NULL, TRUE);
    }
}
	
	void vCodeView::setSyntaxPath(const std::wstring& syntaxPath) {
        m_syntaxPath = syntaxPath;

        // 1. Încărcăm folderul de sintaxe (care populează mapa m_extMap)
        std::string pathAnsi(syntaxPath.begin(), syntaxPath.end());
        m_lexer.loadSyntaxes(pathAnsi);

        // 2. 🔥 REPARAT: Îi spunem lexerului să activeze limbajul bazat pe numele fișierului de sintaxă!
        // Dacă ai funcția setLanguageByFile, o putem păcăli trimițându-i un nume fictiv cu extensia corectă.
        // De exemplu, dacă calea este ".../oli.xml", putem folosi o extensie temporară sau o funcție directă.
        if (syntaxPath.find(L"oli.xml") != std::wstring::npos) {
            m_lexer.setLanguageByFile(L"dummy.oli"); // Forțează activarea sintaxei de Oli
        }
        else if (syntaxPath.find(L"cpp.xml") != std::wstring::npos) {
            m_lexer.setLanguageByFile(L"dummy.cpp"); // Forțează activarea sintaxei de C++
        }
        else {
            // Fallback: încearcă să detecteze limbajul direct prin calea fișierului
            m_lexer.setLanguageByFile(syntaxPath);
        }

        if (m_richEdit) {
            SendMessage(m_richEdit->getHandle(), WM_SETREDRAW, FALSE, 0);
            m_lexer.highlight(m_richEdit);
            SendMessage(m_richEdit->getHandle(), WM_SETREDRAW, TRUE, 0);
            InvalidateRect(m_richEdit->getHandle(), NULL, TRUE);
        }
    }
	
	
    void vCodeView::moveAndResize(int x, int y, int width, int height) {
        // vPanel stochează noile valori în m_width și m_height
        vPanel::moveAndResize(x, y, width, height);

        // Executăm poziționarea fixă a copilului RichEdit adaptată la noile dimensiuni (inclusiv Maximize!)
        this->applyLayout();
    }
    

    void vCodeView::triggerHighlight() {
        if (!m_richEdit || !m_richEdit->getHandle()) return;
        HWND hEdit = m_richEdit->getHandle();

        // 1. 🔥 BLOCĂM REDESENAREA: Înghețăm controlul grafic pentru a preveni flicker-ul
        ::SendMessageW(hEdit, WM_SETREDRAW, FALSE, 0);

        // 2. 🔥 SALVĂM COORDONATELE DE SCROLL: Reținem poziția exactă în pixeli a ferestrei
        POINT scrollPos = { 0, 0 };
        ::SendMessageW(hEdit, EM_GETSCROLLPOS, 0, (LPARAM)&scrollPos);

        // 3. SALVĂM POZIȚIA CURSORULUI
        CHARRANGE cr;
        ::SendMessageW(hEdit, EM_EXGETSEL, 0, (LPARAM)&cr);

        // 4. EXECUTĂM HIGHLIGHT-UL (Lexerul poate face acum selecții oriunde, RichEdit-ul e complet mut)
        m_lexer.highlight(m_richEdit);

        // 5. RESTAURĂM CURSORUL EXACT UNDE ERA
        ::SendMessageW(hEdit, EM_EXSETSEL, 0, (LPARAM)&cr);

        // 6. 🔥 RESTAURĂM COORDONATELE DE SCROLL: Forțăm textul de sus să rămână nemișcat la punct fix
        ::SendMessageW(hEdit, EM_SETSCROLLPOS, 0, (LPARAM)&scrollPos);

        // 7. DEBLOCĂM CONTROLUL ȘI REÎMPROSPĂTĂM
        ::SendMessageW(hEdit, WM_SETREDRAW, TRUE, 0);
        ::InvalidateRect(hEdit, NULL, TRUE);
    }
	
	
void vCodeView::initContextMenu() {
    // 🔥 Folosim ID-ul unic al vCodeView pentru a crea un ID unic de meniu
    std::string uniqueMenuId = "ctx_menu_" + m_id;

    m_contextMenu = std::make_unique<vPopupMenu>(uniqueMenuId, m_dispatcher);

    // Adăugăm itemele
    m_contextMenu->addItem("ctx_cut_" + m_id, L"Cut");
    m_contextMenu->addItem("ctx_copy_" + m_id, L"Copy");
    m_contextMenu->addItem("ctx_paste_" + m_id, L"Paste");
    m_contextMenu->addSeparator("sep_" + m_id);
    m_contextMenu->addItem("ctx_delete_" + m_id, L"Delete");
	m_contextMenu->addSeparator("sep1_" + m_id);
	m_contextMenu->addItem("ctx_comment_" + m_id, L"Comment Line");   // 🔥 Nou
    m_contextMenu->addItem("ctx_uncomment_" + m_id, L"Uncomment Line"); // 🔥 Nou
    // Important: creăm meniul
    m_contextMenu->create(m_handle);
}

void vCodeView::showContextMenu(int x, int y) {
    if (!m_contextMenu) return;

    // Afișăm meniul și primim Win32 ID-ul returnat
    int cmd = m_contextMenu->display(m_handle, x, y);
    if (cmd == 0) return; // Utilizatorul a dat click în altă parte

    HWND hEdit = m_richEdit->getHandle();
    SetFocus(hEdit);

    // 🔥 PRELUĂM ID-URILE DINAMIC
    // Folosim metodele clasei pentru a întreba care este ID-ul numeric 
    // pentru string-ul nostru de identificare ("ctx_cut", etc.)
    int idCut = m_contextMenu->getMenuItemId("ctx_cut_" + m_id);
    int idCopy = m_contextMenu->getMenuItemId("ctx_copy_" + m_id);
    int idPaste = m_contextMenu->getMenuItemId("ctx_paste_" + m_id);
    int idDelete = m_contextMenu->getMenuItemId("ctx_delete_" + m_id);
	int idComment = m_contextMenu->getMenuItemId("ctx_comment_" + m_id);
    int idUncomment = m_contextMenu->getMenuItemId("ctx_uncomment_" + m_id);

    // Comparăm ID-ul returnat de meniu cu ID-urile noastre
    if (cmd == idCut) {
        SendMessage(hEdit, WM_CUT, 0, 0);
    }
    else if (cmd == idCopy) {
        SendMessage(hEdit, WM_COPY, 0, 0);
    }
    else if (cmd == idPaste) {
        SendMessage(hEdit, WM_PASTE, 0, 0);
    }
    else if (cmd == idDelete) {
        SendMessage(hEdit, WM_CLEAR, 0, 0);
    }
	else if (cmd == idComment) {
        this->commentLines(true);
    }
    else if (cmd == idUncomment) {
        this->commentLines(false);
    }
}



void vCodeView::commentLines(bool comment) {
    if (!m_richEdit || !m_richEdit->getHandle()) return;
    HWND hEdit = m_richEdit->getHandle();

    CHARRANGE cr;
    SendMessage(hEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
    
    int len = cr.cpMax - cr.cpMin;
    if (len <= 0) return;

    std::vector<wchar_t> buffer(len + 1);
    TEXTRANGEW tr;
    tr.chrg = cr;
    tr.lpstrText = buffer.data();
    SendMessageW(hEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    std::wstring text(buffer.data());

    // 1. Împărțim textul în linii, eliminând \r și \n
    std::vector<std::wstring> lines;
    std::wstringstream ss(text);
    std::wstring line;
    while (std::getline(ss, line, L'\n')) {
        // Eliminăm manual și \r dacă există (pentru a curăța linia)
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        lines.push_back(line);
    }

    // 2. Procesăm liniile
    std::wstring newText;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (comment) {
            // Adăugăm # doar dacă nu este deja comentată (opțional, dar recomandat)
            if (lines[i].find(L"#") != 0) {
                newText += L"# " + lines[i];
            } else {
                newText += lines[i]; // Deja comentată
            }
        } else {
            // Uncomment: Eliminăm '#' și spațiul de după
            if (lines[i].find(L"# ") == 0) {
                newText += lines[i].substr(2);
            } else if (lines[i].find(L"#") == 0) {
                newText += lines[i].substr(1);
            } else {
                newText += lines[i];
            }
        }
        
        // Adăugăm \r\n înapoi (Windows style) pentru toate liniile
        if (i < lines.size() - 1) newText += L"\r\n";
    }

    // 3. Înlocuim
    SendMessageW(hEdit, EM_REPLACESEL, TRUE, (LPARAM)newText.c_str());
    m_lexer.highlight(m_richEdit);
}
*/