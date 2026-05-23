#ifndef VCOMBOBOX_HPP
#define VCOMBOBOX_HPP

#pragma once

#include "vControl.hpp"
#include <string>
#include <vector> // Pentru stocarea elementelor dacă nu te bazezi doar pe WinAPI
#include <map>    // Poate pentru a stoca date asociate, dacă nu folosești direct LPARAM

class vComboBox : public vControl {
public:
    // Constructor (similar cu vButton)
    explicit vComboBox(HINSTANCE hInstance, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher, int dropdownHeight);
    explicit vComboBox(HINSTANCE hInstance, const std::string& id, EventDispatcher& dispatcher)
        : vControl(hInstance, id, 0, 0, 100, 30, dispatcher), m_dropdownHeight(150) {}

    virtual ~vComboBox() = default;

    void create(HWND parent) override;
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    // Metoda pentru a adăuga elemente
    void addItem(const std::wstring& text, LPARAM itemData = 0); // Permite și date asociate
    void clearItems();

    // Metode pentru a obține informații despre selecție
    int getSelectedIndex() const;
    std::wstring getSelectedText() const;
    LPARAM getSelectedItemData() const;
    void setSelectedIndex(int index);

    void scale(int newDpi) override;

    void setText(const std::wstring& text);

    std::wstring getText() const { return  getSelectedText(); }

protected:
    // Metodă pentru a declanșa evenimentul de schimbare a selecției
    // Aceasta va înlocui conceptual onClick pentru acest control.
    void onSelectionChange();

private:

    int m_dropdownHeight; // Înălțimea totală (bara + lista deschisă) la 96 DPI
    //HINSTANCE m_hInstance;
    //int m_x, m_y, m_width, m_height;

    // Poți folosi membrii pentru a ține minte și elementele,
    // deși ComboBox-ul WinAPI le gestionează intern.
    // Dar e util dacă vrei să ai o copie locală sau să mapezi la obiecte complexe.
    // std::vector<std::pair<std::wstring, LPARAM>> m_items;
};

#endif // VCOMBOBOX_HPP