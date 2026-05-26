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
    
    // Dacă lățimea s-a schimbat semnificativ, actualizăm layout-ul
    if (abs(neededWidth - m_gutterWidth) > 5) {
        m_gutterWidth = neededWidth;
        
        if (m_richEdit) {
            // AICI E SECRETUL:
            // Marginea setată RichEdit-ului trebuie să fie IDENTICĂ cu m_gutterWidth
            m_richEdit->setMargins(m_gutterWidth, 0, 0, 0);
        }
        applyLayout(); 
    }

    // Desenarea propriu-zisă
    HWND hwndPanel = this->getHandle();
    HDC hdcPanel = GetDC(hwndPanel);
    drawLineNumbers(hdcPanel);
    ReleaseDC(hwndPanel, hdcPanel);
}


void vCodeView::create(HWND parent)  {
    vPanel::create(parent);
    setLayoutStrategy(std::make_unique<AnchorLayout>());

    // 1. Creăm RichEdit-ul decalat spre dreapta prin margini
    auto rich = std::make_unique<vRichEdit>(m_hInstance, m_id + "_edit", 0, 0, m_width, m_height, getEventDispatcher());
    m_richEdit = rich.get();
	
	
	m_richEdit->setMargins(5, 0, 0, 0);
    m_richEdit->setHeightMode(SizeMode::FILL);
    m_richEdit->setWidthMode(SizeMode::FILL);
    m_richEdit->setFontSize(m_fontSize);
    //m_richEdit->setMargins(m_gutterWidth, 0, 0, 0);
	

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

    // Notă istorică: Dacă scriptul cere modificarea proprietății "text", 
    // apelul va fi trimis mai jos la vPanel::setProperty, care va ajunge în vControl::setProperty.
    // Acolo se va apela `this->setText()`. Deoarece setText() este virtuală și suprascrisă 
    // chiar aici în vCodeView (cea cu WM_SETREDRAW și m_lexer.highlight), 
    // se va executa automat varianta ta corectă cu highlight inclus!
    
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
            // 1. Dezactivăm temporar redesenarea (redresarea grafică) pentru a evita pâlpâitul (flicker)
            SendMessage(m_richEdit->getHandle(), WM_SETREDRAW, FALSE, 0);

            // 2. Setăm textul brut în controlul RichEdit
            m_richEdit->setText(text);

            // 3. Rulăm Lexer-ul pentru a colora textul conform limbajului curent detectat
            m_lexer.highlight(m_richEdit);

            // 4. Reactivăm redesenarea și forțăm un refresh vizual complet
            SendMessage(m_richEdit->getHandle(), WM_SETREDRAW, TRUE, 0);
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
			// Loghează dimensiunea
			std::wstring msg = L"[vCodeView] Resizing ID: " + str_to_wstr(m_id) + 
							   L" la " + std::to_wstring(width) + L"x" + std::to_wstring(height);
			ConsoleManager::getInstance().log(msg);
			
			vPanel::moveAndResize(x, y, width, height);
			if (m_richEdit) m_richEdit->moveAndResize(0, 0, width, height);
}