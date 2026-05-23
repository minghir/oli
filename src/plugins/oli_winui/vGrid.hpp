#ifndef VGRID_HPP
#define VGRID_HPP

/*
#ifndef WINVER
#define WINVER 0x0601       // Windows 7 sau mai nou
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
*/

#pragma once

#include "vControl.hpp"
#include "ConsoleManager.hpp"

#include <string>
#include <vector>
#include <unordered_set>


class vGrid : public vControl {
public:

    // Constructor
    explicit vGrid(HINSTANCE hInstance, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher);

    // Destructor
    virtual ~vGrid();

    // Suprascrie metoda `create` pentru a crea controlul WinAPI real (SysListView32).
    void create(HWND parent) override;

    // Suprascrie `handleMessage` pentru a gestiona mesaje specifice.
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    // Metodă pentru a adăuga o coloană la Grid.
    // text: textul afișat în antetul coloanei
    // width: lățimea coloanei în pixeli
    // returnează indexul coloanei adăugate, sau -1 în caz de eroare
    int addColumn(const std::wstring& text, int width, bool resizable = true);

    // Metodă pentru a adăuga un rând (un item și sub-item-uri).
    // items: un vector de string-uri, unde primul element este item-ul principal
    // și restul sunt sub-item-uri pentru coloanele următoare.
    // returnează indexul rândului adăugat, sau -1 în caz de eroare
    int addRow(const std::vector<std::wstring>& items);
    
    // TODO: Metode suplimentare pentru a obține/seta valoarea unei celule, a șterge rânduri, etc.
    int getTotalWidth() const;

    
    
    int getColumnX(int colIndex) const; // New public method
    int getColumnWidth(int colIndex) const;

    bool isColumnFixed(int index) const;

    void resize();
    
    void scale(int newDpi) override;
    void updateSortArrow(int columnIndex, bool ascending);

    int getSelectedRow() const;

    std::wstring getCellText(int rowIndex, int colIndex) const;
    void autoFitAllColumns();


protected:

    void autoFitColumn(int columnIndex);
    
    // --- Hook-uri pentru evenimente (pot fi suprascrise) ---

    // Se apelează când se dă click pe antetul unei coloane (pentru sortare, de ex.)
    virtual void onColumnClick(int columnIndex) {
        LOG_DEBUG(L"[vGrid] Column clicked: " + std::to_wstring(columnIndex));
    }

    // Se apelează când se dă dublu click pe un rând
    virtual void onRowDoubleClick(int rowIndex, int colIndex); // Am adăugat colIndex

    void onRowRightClick(int rowIndex, int x, int y);

    // Se apelează când se schimbă selecția
    virtual void onSelectionChanged(int rowIndex) {
        // Poate declanșa un eveniment în dispatcher
    }

    // Se apelează când se dă click dreapta (pentru meniuri contextuale)
    virtual void onContextMenu(int rowIndex, int x, int y) {}

    void applyBoldHeader();
private:
    HFONT m_hFontBold = nullptr;
protected:
    int m_columnIndex = 0; // Pentru a ține evidența numărului de coloane adăugate
    std::unordered_set<int> m_fixedColumns;
    std::vector<int> m_baseColumnWidths; // Stocăm lățimile originale aici
    int m_sortedColumn = -1;      // Adaugă asta
    bool m_sortAscending = true;   // Adaugă asta

};

#endif // VGRID_HPP