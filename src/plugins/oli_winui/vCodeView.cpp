#include "vCodeView.hpp"
#include "ConsoleManager.hpp" 
#include "stringUtils.hpp"
#include <fstream>
#include <sstream>
#include <algorithm> // Pentru std::replace (folosit în mod standard)


#include <richedit.h>


#include <fstream>
#include <sstream>
#include <algorithm>
#include <iterator> // Pentru std::istreambuf_iterator
#include <filesystem>

#include <commctrl.h>
// Presupunând că această funcție este definită și disponibilă
extern std::wstring utf8_to_wstring(const std::string& str);




LRESULT CALLBACK RichEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    vCodeView* codeView = reinterpret_cast<vCodeView*>(dwRefData);

    // Prindem orice mesaj care ar trebui să miște sau să modifice textul
    if (msg == WM_VSCROLL || msg == WM_MOUSEWHEEL || msg == WM_PAINT || msg == WM_CHAR) {
        // Lăsăm mai întâi RichEdit-ul să-și facă scroll-ul intern nativ
        LRESULT res = DefSubclassProc(hwnd, msg, wParam, lParam);

        // Forțăm imediat Panel-ul să-și redeseneze cifrele în stânga
        if (codeView) {
            codeView->redrawGutter();
        }
        return res;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}



// Presupunând că str_to_wstr este disponibil pentru logare
// extern std::wstring str_to_wstr(const std::string& s); 

bool vCodeView::loadFromFile(const std::wstring& filePath) {
    ConsoleManager::getInstance().log(L"[vCodeView::loadFromFile] Se încarcă fișierul: " + filePath);

    //std::ifstream ifs(filePath, std::ios::binary);
    std::ifstream ifs{ std::filesystem::path(filePath), std::ios::binary };
    if (!ifs.is_open()) {
        ConsoleManager::getInstance().log(L"[ERROR] Nu s-a putut deschide fișierul: " + filePath);
        return false;
    }

    std::string utf8Content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    if (utf8Content.empty()) {
        if (m_richEdit) m_richEdit->setText(L"");
        return true;
    }

    std::wstring fileContent = utf8_to_wstring(utf8Content);

    // Normalizare \r\n (Perfectă pentru RichEdit/Edit)
    fileContent.erase(std::remove(fileContent.begin(), fileContent.end(), L'\r'), fileContent.end());

    std::wstring normalizedContent;
    normalizedContent.reserve(fileContent.size() + 100);
    for (wchar_t c : fileContent) {
        if (c == L'\n') normalizedContent += L'\r';
        normalizedContent += c;
    }

    // --- MODIFICARE AICI ---
    // În loc de setText(L""), trimitem către editorul intern
    if (m_richEdit) {
        m_richEdit->setText(normalizedContent);
    }
    // -----------------------

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
    m_fontSize = size;

    if (m_richEdit && m_richEdit->getHandle()) {
        // 1. Aplicăm la RichEdit
        // Framework-ul tău probabil are deja o metodă în vRichEdit
        m_richEdit->setFontSize(size);

        // 2. Aplicăm la Gutter (când îl vei activa)
        /*
        if (m_lineGutter) {
            m_lineGutter->updateFont(L"Consolas", size);
        }
        */

        // 3. Forțăm un re-layout pentru că schimbarea fontului poate modifica 
        // lățimea necesară pentru Gutter (ex: de la 99 la 100 linii)
        applyLayout();

        // 4. Re-colorează (uneori RichEdit pierde formatarea la schimbări majore de font)
         //applayColors(); 
    }
}



LRESULT vCodeView::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        // Interceptăm momentul în care Windows vrea să șteargă fundalul ca să nu ne dea flicker sau să șteargă cifrele
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        // Ștergem/colorăm doar zona din DREAPTA (unde e RichEdit-ul), lăsând Gutter-ul în pace
        RECT richEditArea = { m_gutterWidth, rcClient.top, rcClient.right, rcClient.bottom };
        HBRUSH hBgBrush = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
        FillRect(hdc, &richEditArea, hBgBrush);
        DeleteObject(hBgBrush);

        // Desenăm instantaneu Gutter-ul în zona protejată din stânga
        drawLineNumbers(hdc);
        return 1; // Îi spunem lui Windows că am șters noi fundalul
    }

    case WM_PAINT: {
        // Lăsăm panelul să-și facă treaba
        LRESULT res = vPanel::handleMessage(hwnd, msg, wParam, lParam);

        // Forțăm desenarea numerelor imediat după paint-ul standard
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        drawLineNumbers(hdc);
        EndPaint(hwnd, &ps);
        return res;
    }

    case WM_NOTIFY: {
        LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>(lParam);
        if (m_richEdit && nmhdr->hwndFrom == m_richEdit->getHandle()) {
            if (nmhdr->code == EN_VSCROLL || nmhdr->code == EN_CHANGE) {
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

    // 1. Desenăm fundalul Gutter-ului direct cu ALB (la fel ca RichEdit)
    RECT gutterRect = { 0, 0, m_gutterWidth, m_height };
    HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255)); // <--- SCHIMBAT ÎN ALB COMPLET
    FillRect(hdc, &gutterRect, hBrush);
    DeleteObject(hBrush);

    // 2. Linia de demarcație verticală
    // O facem tot ALBI dacă vrei să dispară de tot, sau un gri extrem de șters (RGB(240,240,240)) ca un ghidaj fin
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255)); // <--- SCHIMBAT ÎN ALB (dispare linia)
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, m_gutterWidth - 1, 0, nullptr);
    LineTo(hdc, m_gutterWidth - 1, m_height);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    // 3. Setări text (numerele rămân pe gri închis ca să fie lizibile, dar pe fundal alb)
    SetTextColor(hdc, RGB(140, 140, 140)); // Un gri curat pentru cifre
    SetBkMode(hdc, TRANSPARENT);

    // --- CORECTURA 1: Forțăm utilizarea fontului din framework-ul tău ---
    // Încercăm să luăm fontul obiectului vCodeView/vPanel. Dacă nu e setat, îl luăm pe cel din RichEdit
    HFONT hMyFont = m_richEdit->getFont();
    if (!hMyFont) {
        hMyFont = (HFONT)SendMessage(hEdit, WM_GETFONT, 0, 0);
    }

    HFONT hOldFont = nullptr;
    if (hMyFont) {
        hOldFont = (HFONT)SelectObject(hdc, hMyFont);
    }

    // 3. Aflăm pozițiile de scroll
    int firstLine = (int)SendMessage(hEdit, EM_GETFIRSTVISIBLELINE, 0, 0);
    int totalLines = (int)SendMessage(hEdit, EM_GETLINECOUNT, 0, 0);

    // 4. Calculăm înălțimea liniei direct din contextul fontului aplicat
    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    if (lineHeight <= 0) lineHeight = 16;

    // --- CORECTURA 2: Aliniere la pixel prin interogarea directă a liniei vizibile ---
    // Luăm indexul primului caracter de pe linia curentă de sus
    int firstCharIndex = (int)SendMessage(hEdit, EM_LINEINDEX, firstLine, 0);
    int yPos = 0;

    if (firstCharIndex != -1) {
        POINT pt = { 0, 0 };
        // Trimitem pointerul corect către structura POINT (WPARAM) și indexul caracterului (LPARAM)
        // Asta funcționează perfect în RichEdit și ne spune EXACT la ce pixel Y începe textul pe ecran
        SendMessage(hEdit, EM_POSFROMCHAR, (WPARAM)&pt, firstCharIndex);
        yPos = pt.y;
    }

    // 5. Bucla de randare

    SetTextAlign(hdc, TA_RIGHT | TA_TOP);

    int currentLine = firstLine;

    while (yPos < m_height && currentLine < totalLines) {
        std::wstring lineNumStr = std::to_wstring(currentLine + 1);

        // Pasăm coordonata X fixă: m_gutterWidth - 8 (adică exact la 8 pixeli în stânga de RichEdit)
        // Datorită TA_RIGHT, cifrele se vor extinde spre stânga, rămânând lipite frumos de RichEdit!
        TextOutW(hdc, m_gutterWidth - 8, yPos, lineNumStr.c_str(), static_cast<int>(lineNumStr.length()));

        currentLine++;
        int nextCharIndex = (int)SendMessage(hEdit, EM_LINEINDEX, currentLine, 0);
        if (nextCharIndex != -1) {
            POINT ptNext = { 0, 0 };
            SendMessage(hEdit, EM_POSFROMCHAR, (WPARAM)&ptNext, nextCharIndex);
            yPos = ptNext.y;
        }
        else {
            yPos += lineHeight;
        }
    }

    if (hOldFont) {
        SelectObject(hdc, hOldFont);
    }
}

void vCodeView::redrawGutter() {
    HWND hwndPanel = this->getHandle(); // Handle-ul PANELULUI vCodeView
    if (!hwndPanel) return;

    // Luăm DC-ul direct, ocolind mesajul WM_PAINT
    HDC hdcPanel = GetDC(hwndPanel);

    // Rulăm funcția ta de randare
    drawLineNumbers(hdcPanel);

    // Eliberăm resursele grafice
    ReleaseDC(hwndPanel, hdcPanel);
}


void vCodeView::create(HWND parent)  {
    vPanel::create(parent);
    setLayoutStrategy(std::make_unique<AnchorLayout>());

    // 1. Creăm RichEdit-ul decalat spre dreapta prin margini
    auto rich = std::make_unique<vRichEdit>(m_hInstance, m_id + "_edit", 0, 0, m_width, m_height, getEventDispatcher());
    m_richEdit = rich.get();

    m_richEdit->setHeightMode(SizeMode::FILL);
    m_richEdit->setWidthMode(SizeMode::FILL);
    m_richEdit->setFontSize(m_fontSize);
    m_richEdit->setMargins(m_gutterWidth, 0, 0, 0);

    this->addChild(m_id + "_edit", std::move(rich));

    // --- LEGĂTURA CRUCIALĂ RE-ACTIVATĂ ---
    if (m_richEdit->getHandle()) {
        HWND hEdit = m_richEdit->getHandle();
        // Îi spunem controlului RichEdit să execute RichEditSubclassProc la scroll/taste
        SetWindowSubclass(hEdit, RichEditSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));

        // Activăm măștile de notificare pentru orice eventualitate
        SendMessage(hEdit, EM_SETEVENTMASK, 0, ENM_SCROLL | ENM_CHANGE);
    }

    applyLayout();

    // Forțăm o primă redesenare curată
    InvalidateRect(this->getHandle(), NULL, TRUE);
}