
#pragma once

// Plaseaza aceste macro-uri la inceputul fisierului, inainte de orice includere de antet Windows
//#define WINVER 0x0600
//#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <shellscalingapi.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

#include "vGrid.hpp"
#include "vPopupMenu.hpp"
#include "FontManager.hpp"
#include "../../ConsoleManager.hpp"
#include "stringUtils.hpp"

#ifndef LVS_EX_COLUMNSHADOWS
#define LVS_EX_COLUMNSHADOWS 0x00008000
#endif

#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

// Constructor
vGrid::vGrid(HINSTANCE hInstance, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher)
    : vControl(hInstance, id,x,y, width, height, dispatcher)
     {
  //  ConsoleManager::getInstance().log(L"[vGrid::Constructor] Apelat pentru ID: " + std::wstring(id.begin(), id.end()));
}

vGrid::~vGrid() {
    if (m_hFontBold) {
        DeleteObject(m_hFontBold);
    }
}

// Metoda create - creează controlul ListView
void vGrid::create(HWND parent) {
   // ConsoleManager::getInstance().log(L"[vGrid::create] Creare Grid cu ID: " + std::wstring(m_id.begin(), m_id.end()) + L" în părinte HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(parent)));

    if (!parent) {
        ConsoleManager::getInstance().log(L"[ERROR] vGrid::create: Părintele HWND este nullptr. Grid-ul nu poate fi creat.");
        return;
    }

    // Aici se inițializează controalele comune, un pas obligatoriu pentru ListView.
    // Această inițializare trebuie făcută o singură dată la începutul aplicației (e.g., în vApp::init()).
    // Dacă ai făcut-o deja, poți scoate acest bloc de cod.
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
   // InitCommonControlsEx(&icex);
    UINT parentDpi = GetDpiForWindow(parent);
    // Apelează metoda de scalare cu DPI-ul obținut
    scale(parentDpi);
    // Pasul 1: Creează handle-ul WinAPI (m_handle) pentru controlul ListView.
    m_handle = CreateWindowExW(
        WS_EX_CLIENTEDGE,                                                     // Stiluri extinse: margine 3D
        WC_LISTVIEWW,                                                         // Clasa de fereastră standard pentru ListView
        L"",                                                                  // Textul inițial este gol
        WS_CHILD | WS_VISIBLE | LVS_REPORT |  LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_VSCROLL | WS_HSCROLL,  // Stiluri: copil, vizibil, raport (coloane/rânduri), etc.
        getX(), getY(), getWidth(), getHeight(),                              // Poziție și dimensiune
        parent,                                                               // Fereastra părinte
        (HMENU)(uintptr_t)getWin32Id(),                                       // ID-ul controlului
        m_hInstance,                                                          // Handle-ul instanței
        this                                                                  // Pointer către obiectul vGrid
       
    );

    if (!m_handle) {
        ConsoleManager::getInstance().log(L"[ERROR] Creare vGrid: Eroare la crearea HWND-ului. Cod eroare: " + std::to_wstring(GetLastError()));
        return;
    }
    

    HFONT hFontNormal = FontManager::getInstance().getFont(
        m_fontName,
        -MulDiv(m_baseFontSize, parentDpi, 72),
        FW_NORMAL
    );

    if (hFontNormal) {
        SendMessage(m_handle, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
    }
    
    
    // Setează stilurile extinse după crearea controlului.
    ListView_SetExtendedListViewStyle(m_handle, LVS_EX_FULLROWSELECT | 
                                                LVS_EX_GRIDLINES | 
                                                LVS_EX_COLUMNSHADOWS |
                                                LVS_EX_DOUBLEBUFFER 
            );

    //HWND hHeader = ListView_GetHeader(m_handle);
   
    // Obținem fontul curent și creăm o variantă BOLD a acestuia
    //m_hFontBold = FontManager::getInstance().getScaledFont(L"Segoe UI", 10, GetDpiForWindow(parent));
    m_hFontBold = FontManager::getInstance().getFont(m_fontName, -MulDiv(m_baseFontSize, parentDpi,  72), FW_BOLD);

    HWND hHeader = ListView_GetHeader(m_handle);
    if (hHeader && m_hFontBold) {
        SendMessage(hHeader, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
        DWORD dwStyle = GetWindowLong(hHeader, GWL_STYLE);
        SetWindowLong(hHeader, GWL_STYLE, dwStyle | HDS_BUTTONS);
    }

  //  ConsoleManager::getInstance().log(L"[vGrid::create] Grid-ul (ID: " + std::wstring(m_id.begin(), m_id.end()) + L") a fost creat cu succes. HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(m_handle)));
}


LRESULT vGrid::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

    
  
    if (msg == WM_NOTIFY) {
        LPNMHDR lpnmhdr = reinterpret_cast<LPNMHDR>(lParam);
        HWND hHeader = ListView_GetHeader(m_handle);

        if (lpnmhdr->hwndFrom == m_handle || lpnmhdr->hwndFrom == hHeader) {
            UINT cod = static_cast<int>(lpnmhdr->code);

            switch (cod) {
            
           

                // --- Logica de desenare personalizată (Culoare Selecție) ---
            case NM_CUSTOMDRAW: {
                LPNMHDR pnmh = (LPNMHDR)lParam;
                HWND hHeader = ListView_GetHeader(m_handle);

                // --- 1. LOGICA PENTRU HEADER (BOLD) ---
                if (pnmh->hwndFrom == hHeader) {
                    LPNMCUSTOMDRAW lpnmcd = (LPNMCUSTOMDRAW)lParam;
                    switch (lpnmcd->dwDrawStage) {
                    case CDDS_PREPAINT:
                        return CDRF_NOTIFYITEMDRAW;
                    case CDDS_ITEMPREPAINT:
                        // Selectăm fontul bold creat în create()
                        if (m_hFontBold) {
                            SelectObject(lpnmcd->hdc, m_hFontBold);
                            return CDRF_NEWFONT;
                        }
                        break;
                    }
                    return CDRF_DODEFAULT;
                }


                LPNMLVCUSTOMDRAW lplvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam);

                if (lplvcd->nmcd.hdr.hwndFrom == m_handle) {
                    
                    // --- LOGICA DE DETECTARE SCROLL ---
        // Verificăm doar la nivel de PREPAINT pentru a nu repeta logica de mii de ori pe secundă
                    if (lplvcd->nmcd.dwDrawStage == CDDS_PREPAINT) {
                        static int lastX = -1;
                        int currentX = GetScrollPos(m_handle, SB_HORZ);

                        if (currentX != lastX) {
                            lastX = currentX;
                            // Notificăm părintele (vDbFilteredGrid)
                            PostMessage(GetParent(m_handle), WM_USER + 101, (WPARAM)currentX, 0);
                        }
                    }
                    // --- SFÂRȘIT LOGICA SCROLL ---
                    

                    switch (lplvcd->nmcd.dwDrawStage) {

                        case CDDS_PREPAINT:
                            return CDRF_NOTIFYITEMDRAW;

                        case CDDS_ITEMPREPAINT:
                            return CDRF_NOTIFYSUBITEMDRAW;

                        case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
                            int iRow = static_cast<int>(lplvcd->nmcd.dwItemSpec);
                            int iCol = lplvcd->iSubItem;

                            // 1. Gestionare Rând Selectat (Prioritate maximă)
                            if (ListView_GetItemState(m_handle, iRow, LVIS_SELECTED) & LVIS_SELECTED) {
                                lplvcd->nmcd.uItemState &= ~CDIS_SELECTED;
                                lplvcd->nmcd.uItemState &= ~CDIS_FOCUS;
                                lplvcd->clrTextBk = RGB(230, 230, 230); // Gri selecție
                                lplvcd->clrText = RGB(0, 0, 0);
                                return CDRF_NEWFONT;
                            }

                            // 2. Gestionare Coloană Sortată
                            if (iCol == m_sortedColumn) {
                                lplvcd->clrTextBk = RGB(230, 230, 230);
                                lplvcd->clrText = RGB(0, 0, 0); // Text negru clar
                                return CDRF_NEWFONT;
                            }
                            else {
                                // IMPORTANT: Resetăm la culorile implicite pentru restul coloanelor
                                // Fără acest else, Windows "reutilizează" culoarea de la coloana anterioară
                                lplvcd->clrTextBk = ListView_GetBkColor(m_handle);
                                lplvcd->clrText = ListView_GetTextColor(m_handle);
                                return CDRF_NEWFONT;
                            }
                        }
                    }
                }
                break;
            }

                              // --- Logica de redimensionare (Coloane Fixe) ---

            case HDN_DIVIDERDBLCLICKA:
            case HDN_DIVIDERDBLCLICKW: {
                LPNMHEADER pnmh = reinterpret_cast<LPNMHEADER>(lParam);
                if (!isColumnFixed(pnmh->iItem)) {
                    
                    autoFitColumn(pnmh->iItem);
                    PostMessage(GetParent(m_handle), WM_USER + 102, 0, 0);
                    return TRUE; // Spunem că am gestionat noi evenimentul
                }
                break;
            }
            case HDN_TRACKW:
            case HDN_TRACKA:
            case HDN_BEGINTRACKA:
            case HDN_ITEMCHANGINGA:
            case HDN_BEGINTRACKW:
            case HDN_ITEMCHANGINGW:
            case HDN_ITEMCHANGEDA: {
                LPNMHEADER pnmh = reinterpret_cast<LPNMHEADER>(lParam);
                if (isColumnFixed(pnmh->iItem)) {
                    return TRUE; // Blocăm redimensionarea
                }
                else {
                    PostMessage(GetParent(m_handle), WM_USER + 102, 0, 0);
                }
                break;
            }

                                  // --- Evenimente Virtuale pentru Logică ---
            case LVN_COLUMNCLICK: {
                LPNMLISTVIEW lpnmlv = reinterpret_cast<LPNMLISTVIEW>(lParam);
                onColumnClick(lpnmlv->iSubItem);
                return 0;
            }

            case NM_DBLCLK: {
                LPNMITEMACTIVATE pnmia = reinterpret_cast<LPNMITEMACTIVATE>(lParam);
                if (pnmia->iItem != -1) {
                    // Trimitem atât rândul, cât și coloana
                    onRowDoubleClick(pnmia->iItem, pnmia->iSubItem);
                }
                return 0;
            }

            case NM_RCLICK: {
                LPNMITEMACTIVATE pnmia = reinterpret_cast<LPNMITEMACTIVATE>(lParam);

                // 1. Luăm coordonatele cursorului (necesare pentru TrackPopupMenu)
                POINT pt;
                GetCursorPos(&pt);

                // 2. Verificăm dacă s-a dat click pe un rând valid sau în spațiul gol
                // pnmia->iItem va fi -1 dacă s-a dat click sub rândurile existente
                onRowRightClick(pnmia->iItem, pt.x, pt.y);

                return 0;
            }

            case LVN_ITEMCHANGED: {
                LPNMLISTVIEW pnmv = reinterpret_cast<LPNMLISTVIEW>(lParam);
                if ((pnmv->uChanged & LVIF_STATE) && (pnmv->uNewState & LVIS_SELECTED)) {
                    onSelectionChanged(pnmv->iItem);
                }
                break;
            }
            }
        }
    }

    return vControl::handleMessage(hwnd, msg, wParam, lParam);
}

void vGrid::applyBoldHeader() {
    if (!m_handle) return;

    HWND hHeader = (HWND)SendMessage(m_handle, LVM_GETHEADER, 0, 0);
    if (hHeader) {
        // Îi spunem Windows-ului să nu mai aplice stilul vizual standard pe Header
        // Acest lucru permite fontului setat prin WM_SETFONT să fie respectat
        SetWindowTheme(hHeader, L"", L"");

        HFONT hBoldFont = FontManager::getInstance().getScaledFont(
            m_fontName, m_baseFontSize, m_currentDpi, FW_BOLD, false, false
        );

        if (hBoldFont) {
            SendMessage(hHeader, WM_SETFONT, (WPARAM)hBoldFont, TRUE);
        }
    }
}

int vGrid::addColumn(const std::wstring& text, int width, bool resizable) {
    if (!m_handle) return -1;

    // 1. Salvăm lățimea de bază (originală) pentru resize-uri viitoare
    m_baseColumnWidths.push_back(width);

    // 2. Folosim DPI-ul curent al controlului (stocat în vControl)
    int scaledWidth = MulDiv(width, m_currentDpi, 96);

    //LOG_DEBUG(L"Adaug coloana:" + text + L" cu dimensiunea de baza: " + to_wstring<int>(width) + L" si valoarea scalata: " + to_wstring<int>(scaledWidth));

    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT; // Scoate FIXED_WIDTH de aici, îl gestionăm prin m_fixedColumns
    lvc.cx = scaledWidth;
    lvc.pszText = const_cast<LPWSTR>(text.c_str());
    lvc.iSubItem = m_columnIndex;

    int columnIndex = ListView_InsertColumn(m_handle, m_columnIndex, &lvc);

    if (columnIndex != -1) {
        m_columnIndex++;
        if (!resizable) {
            m_fixedColumns.insert(columnIndex);
        }
        applyBoldHeader();
    }
    return columnIndex;
}

// Adaugă un rând de date
int vGrid::addRow(const std::vector<std::wstring>& items) {
    if (items.empty()) {
        return -1;
    }

    LVITEMW lvi;
    lvi.mask = LVIF_TEXT;
    lvi.iItem = ListView_GetItemCount(m_handle); // Adaugă la final
    lvi.iSubItem = 0;
    lvi.pszText = const_cast<LPWSTR>(items[0].c_str());

    int itemIndex = ListView_InsertItem(m_handle, &lvi);

    if (itemIndex != -1) {
        for (size_t i = 1; i < items.size(); ++i) {
            std::wstring text = trim(items[i]);
            //ListView_SetItemTextW(m_handle, itemIndex, static_cast<int>(i), const_cast<LPWSTR>(text.c_str()));
            LVITEMW lvi = { 0 };
            lvi.iSubItem = static_cast<int>(i);
            lvi.pszText = const_cast<LPWSTR>(text.c_str());
            SendMessageW(m_handle, LVM_SETITEMTEXTW, static_cast<WPARAM>(itemIndex), reinterpret_cast<LPARAM>(&lvi));
        }
        //ConsoleManager::getInstance().log(L"[vGrid::addRow] Rând adăugat la index " + std::to_wstring(itemIndex));
    }
    else {
        ConsoleManager::getInstance().log(L"[ERROR] Adăugare rând a eșuat. Cod eroare: " + std::to_wstring(GetLastError()));
    }
   
    return itemIndex;
}

int vGrid::getTotalWidth() const {
    if (!m_handle) {
        return 0;
    }

    int totalWidth = 0;
    int columnCount = ListView_GetHeader(m_handle) ? Header_GetItemCount(ListView_GetHeader(m_handle)) : 0;

    for (int i = 0; i < columnCount; ++i) {
        totalWidth += ListView_GetColumnWidth(m_handle, i);
    }

    return totalWidth;
}


int vGrid::getColumnX(int colIndex) const {
    if (!m_handle || colIndex < 0) {
        return 0;
    }

    int cumulativeWidth = 0;
    for (int i = 0; i < colIndex; ++i) {
        cumulativeWidth += ListView_GetColumnWidth(m_handle, i);
    }
    return cumulativeWidth;
}

int vGrid::getColumnWidth(int colIndex) const {
    if (!m_handle || colIndex < 0) {
        return 0;
    }
    return ListView_GetColumnWidth(m_handle, colIndex);
}

bool vGrid::isColumnFixed(int index) const {
    return m_fixedColumns.find(index) != m_fixedColumns.end();
}

/*
void vGrid::resize() {
    RECT rcGrid;
    GetWindowRect(m_handle, &rcGrid);

  

    vControl::resize();
    if (!m_handle) return;

    


    // 1. Setăm fontul
    HFONT hFont = FontManager::getInstance().getFont(
        m_fontName,
        -MulDiv(m_baseFontSize, m_currentDpi, 72)
    );

    SendMessage(m_handle, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hHeader = (HWND)SendMessage(m_handle, LVM_GETHEADER, 0, 0);
    if (hHeader) {
        HFONT hHeaderFont = FontManager::getInstance().getFont(
            m_fontName,
            -MulDiv(m_baseFontSize, m_currentDpi, 72),
            FW_BOLD
        );
        SendMessage(hHeader, WM_SETFONT, (WPARAM)hHeaderFont, TRUE);
    }

    // 2. Forțăm ListView-ul să proceseze fontul
    UpdateWindow(m_handle);

    // 3. Ajustăm header-ul
    if (hHeader) {
        RECT rect;
        GetClientRect(m_handle, &rect);
        int scaledHeaderHeight = MulDiv(25, m_currentDpi, 96);

        SetWindowPos(hHeader, NULL, 0, 0,
            rect.right - rect.left,
            scaledHeaderHeight,
            SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }

    // 4. Aplicăm lățimile coloanelor
    for (int i = 0; i < (int)m_baseColumnWidths.size(); ++i) {
        
        int scaledWidth = MulDiv(m_baseColumnWidths[i], m_currentDpi, 96);

 // LOG_DEBUG(L"[vGrid] Resize pentru coloana:" + to_wstring<int>(i) + L"de la dim de baza:"+
      //to_wstring<int>(m_baseColumnWidths[i]) +L" la dimensiune:  " + 
      //to_wstring<int>(scaledWidth) + L" pentru DPI: "+ std::to_wstring(m_currentDpi));

        ListView_SetColumnWidth(m_handle, i, scaledWidth);
    }

    InvalidateRect(m_handle, NULL, TRUE);


    GetWindowRect(m_handle, &rcGrid);

   
    if (m_sortedColumn != -1) {
        updateSortArrow(m_sortedColumn, m_sortAscending);
    }
}
*/

void vGrid::resize() {
    vControl::resize(); // Schimbă doar dimensiunea ferestrei

    // Nu mai recalcula fonturi aici, lasă scale() să o facă.
    // Doar asigură-te că Header-ul și coloanele sunt aliniate la dimensiunea ferestrei
    HWND hHeader = ListView_GetHeader(m_handle);
    if (hHeader) {
        PostMessage(hHeader, WM_SIZE, 0, 0);
    }
}

void vGrid::scale(int newDpi) {
    // 1. Apelăm scale din vControl (care va face m_currentDpi = newDpi)
    // și va scala dimensiunile HWND-ului ListView.
    vControl::scale(newDpi);

    if (!m_handle) return;

    // 2. Sincronizăm fontul conținutului (Rândurile)
    // Folosim metoda standard din vControl ca să fim siguri că are aceeași mărime ca restul UI
    this->scaleFont(newDpi);

    // 3. Header-ul (Trebuie să fie BOLD, dar de ACEEAȘI MĂRIME ca restul)
    HWND hHeader = ListView_GetHeader(m_handle);
    if (hHeader) {
        // Distrugem fontul vechi
        if (m_hFontBold) DeleteObject(m_hFontBold);

        // Cerem de la manager un font identic cu cel din rânduri, dar cu FW_BOLD
        m_hFontBold = FontManager::getInstance().getScaledFont(
            m_fontName,
            m_baseFontSize, // Valoarea de bază (ex: 10 sau 12)
            newDpi,
            FW_BOLD,
            m_fontItalic,
            m_fontUnderline
        );

        if (m_hFontBold) {
            SendMessage(hHeader, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);

            // Această linie forțează înălțimea Header-ului să se adapteze
            SetWindowPos(hHeader, NULL, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
    }

    // 4. Scalăm coloanele (asigură-te că baseColumnWidths sunt pixeli la 96 DPI)
    for (int i = 0; i < (int)m_baseColumnWidths.size(); ++i) {
        int scaledW = MulDiv(m_baseColumnWidths[i], newDpi, 96);
        ListView_SetColumnWidth(m_handle, i, scaledW);
    }

    // 5. Trick pentru a forța ListView-ul să recalculeze înălțimea rândurilor
    // Schimbăm stilul temporar (invizibil) pentru a declanșa recalcularea metricilor
    DWORD dwStyle = GetWindowLong(m_handle, GWL_STYLE);
    SetWindowLong(m_handle, GWL_STYLE, dwStyle);

    InvalidateRect(m_handle, NULL, TRUE);
}

void vGrid::updateSortArrow(int columnIndex, bool ascending) {
    m_sortedColumn = columnIndex;
    m_sortAscending = ascending;

    if (!m_handle) return;

    HWND hHeader = ListView_GetHeader(m_handle);
    if (!hHeader) return;

    int columnCount = Header_GetItemCount(hHeader);
    for (int i = 0; i < columnCount; ++i) {
        HDITEMW hdItem = { 0 };
        hdItem.mask = HDI_FORMAT;

        if (Header_GetItem(hHeader, i, &hdItem)) {
            // Păstrăm alinierea originală (stânga/centru/dreapta)
            hdItem.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN | HDF_BITMAP_ON_RIGHT);

            if (i == columnIndex) {
                hdItem.fmt |= (ascending ? HDF_SORTUP : HDF_SORTDOWN);
                // Opțional, forțăm săgeata să fie în dreapta textului
                hdItem.fmt |= HDF_BITMAP_ON_RIGHT;
            }
            Header_SetItem(hHeader, i, &hdItem);
        }
    }

    // Foarte important: Forțează header-ul să se redeseneze, nu doar grid-ul
    InvalidateRect(hHeader, NULL, TRUE);
    InvalidateRect(m_handle, NULL, TRUE);
}

void vGrid::onRowRightClick(int rowIndex, int x, int y) {
    // Împachetăm datele într-un string: "rowIndex;x;y"
    std::string args = std::to_string(rowIndex) + ";" +
        std::to_string(x) + ";" +
        std::to_string(y);

    // Declanșăm evenimentul generic
    // m_id este ID-ul grid-ului (ex: "mainGrid")
    m_dispatcher.dispatch("grid_right_click", m_id, args);
}

int vGrid::getSelectedRow() const {
    if (!m_handle) return -1;

    // LVM_GETNEXTITEM: 
    // wParam: indexul de la care începe căutarea (-1 pentru a începe de la început)
    // lParam: flag-ul de stare (LVNI_SELECTED pentru a găsi rândul selectat)
    return (int)SendMessage(m_handle, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
}

std::wstring vGrid::getCellText(int rowIndex, int colIndex) const {
    if (!m_handle || rowIndex < 0 || colIndex < 0) return L"";

    // Verificăm numărul de coloane existente
    HWND hHeader = ListView_GetHeader(m_handle);
    int colCount = Header_GetItemCount(hHeader);
    if (colIndex >= colCount) return L"";

    wchar_t buffer[2048]; // Buffer generos pentru conținutul celulei
    LVITEMW lvi = { 0 };
    lvi.iSubItem = colIndex;
    lvi.cchTextMax = 2048;
    lvi.pszText = buffer;

    // LVM_GETITEMTEXTW este varianta Unicode (Wide)
    SendMessage(m_handle, LVM_GETITEMTEXTW, (WPARAM)rowIndex, (LPARAM)&lvi);
    return std::wstring(buffer);
}

void vGrid::onRowDoubleClick(int rowIndex, int colIndex) {
    // Luăm textul brut (Unicode)
    std::wstring cellContent = getCellText(rowIndex, colIndex);

    // Îl convertim folosind funcția ta existentă
    std::string utf8Content = wstring_to_utf8(cellContent);

    // Împachetăm argumentele pentru dispatcher
    // Format: "row;col;content"
    std::string args = std::to_string(rowIndex) + ";" +
        std::to_string(colIndex) + ";" +
        utf8Content;

    // Trimitem evenimentul către restul aplicației
    m_dispatcher.dispatch("grid_row_dblclick", m_id, args);

    //LOG_DEBUG(L"[vGrid] DblClick pe celula [" + std::to_wstring(rowIndex) + L"," + std::to_wstring(colIndex) + L"] -> " + cellContent);
}

void vGrid::autoFitColumn(int columnIndex) {
    int maxW = 0;
    HWND hHeader = ListView_GetHeader(m_handle);
    HDC hdc = GetDC(m_handle);

    // Selectăm fontul curent în contextul grafic pentru a măsura corect
    HFONT hOldFont = (HFONT)SelectObject(hdc, (HFONT)SendMessage(m_handle, WM_GETFONT, 0, 0));

    // 1. Măsurăm textul din Header
    wchar_t headerText[256];
    HDITEMW hi = { 0 };
    hi.mask = HDI_TEXT;
    hi.pszText = headerText;
    hi.cchTextMax = 256;
    Header_GetItem(hHeader, columnIndex, &hi);

    SIZE s;
    GetTextExtentPoint32W(hdc, headerText, (int)wcslen(headerText), &s);
    maxW = s.cx + 20; // Adăugăm padding pentru margini și iconița de sortare

    // 2. Măsurăm textul din fiecare rând al coloanei
    int rowCount = ListView_GetItemCount(m_handle);
    for (int i = 0; i < rowCount; ++i) {
        wchar_t cellText[1024];
        LVITEMW lvi{};
        lvi.iSubItem = columnIndex;
        lvi.cchTextMax = 1024;
        lvi.pszText = cellText;
        SendMessageW(m_handle, LVM_GETITEMTEXTW, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&lvi));

        GetTextExtentPoint32W(hdc, cellText, (int)wcslen(cellText), &s);
        if (s.cx + 15 > maxW) {
            maxW = s.cx + 15;
        }
    }

    // Eliberăm resursele grafice
    SelectObject(hdc, hOldFont);
    ReleaseDC(m_handle, hdc);

    // 3. Aplicăm noua lățime coloanei
    ListView_SetColumnWidth(m_handle, columnIndex, maxW);

    // 4. Notificăm vDbFilteredGrid să realinieze filtrele (mesajul 1126 creat anterior)
    PostMessage(GetParent(m_handle), WM_USER + 102, 0, 0);
}

void vGrid::autoFitAllColumns() {
    HWND hHeader = ListView_GetHeader(m_handle);
    int colCount = Header_GetItemCount(hHeader);
    for (int i = 0; i < colCount; ++i) {
        autoFitColumn(i);
    }
}