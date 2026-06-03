#include "vCodeView.hpp"
#include "ConsoleManager.hpp" 
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
        SendMessage(m_richEdit->getHandle(), WM_SETREDRAW, FALSE, 0);
        m_richEdit->setText(text);
        m_lexer.highlight(m_richEdit);
        SendMessage(m_richEdit->getHandle(), WM_SETREDRAW, TRUE, 0);

        // Textul s-a schimbat, deci numărul de linii s-a modificat radical. Actualizăm layout-ul!
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
    /*
    void vCodeView::triggerHighlight() {
        if (!m_richEdit || !m_richEdit->getHandle()) return;
        HWND hEdit = m_richEdit->getHandle();

        // 1. 🔥 SALVĂM CU PRECIZIE POZIȚIA CURSORULUI (CARET POSITION)
        CHARRANGE cr;
        ::SendMessageW(hEdit, EM_EXGETSEL, 0, (LPARAM)&cr);

        // 2. Rulăm lexerul tău pe editor
        // (Presupun că ai o instanță de lexer accesibilă sau un Singleton, adaptează apelul după arhitectura ta)
        // Dacă ai lexer global sau proprietate în vCodeView, îl apelăm așa:
        m_lexer.highlight(m_richEdit);

        // 3. 🔥 RESTAURĂM CURSORUL EXACT UNDE ERA
        // Acest pas elimină complet orice tremurat (flicker) sau jump ilegal al cursorului la tastare
        ::SendMessageW(hEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
    }
    */

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