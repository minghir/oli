#ifndef VMENU_HPP
#define VMENU_HPP

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <utility>

#include "ControlIdManager.hpp"
#include "vControl.hpp"

// Structura pentru a defini un element de meniu
struct MenuItem {
    std::string id;
    std::wstring text;
    int win32Id;
    bool isSeparator;
    bool enabled = true;
};

// Clasa vMenu moștenește vControl
class vMenu : public vControl {
public:
    // Constructor. Asociază un ID unic controlului.
    explicit vMenu(const std::string& id, EventDispatcher& dispatcher);

    // Suprascriem destructorul
    virtual ~vMenu();

    // Implementarea metodei virtuale pure din vControl
    // NOTĂ: Un meniu nu are un "părinte" în sensul HWND,
    // așa că această metodă este diferită de cea a unui control vizual.
    void create(HWND parent) override;

    // Nu mai este nevoie de această metodă, o poți scoate
    // deoarece `create()` este acum cea corectă.
    // void create();

    // Adaugă un element de meniu
    void addItem(const std::string& id, const std::wstring& text);

    // Adaugă un separator
    void addSeparator(const std::string& id);

    // Adaugă un submeniu
    void addSubMenu(const std::wstring& text, vMenu* subMenu);

    // Obține handle-ul WinAPI al meniului
    HMENU getHandle() const { return m_handle; }

    // Obține ID-ul numeric Win32 al unui element de meniu
    int getMenuItemId(const std::string& id) const;

    // Aceasta trebuie adăugată (Int -> String) - NECESARĂ PENTRU CLICK
    std::string getItemIdByWin32Id(int win32Id) const;

    void setEnabled(const std::string& id, bool enabled);
	
	bool setProperty(const std::wstring& name, const vData& value) override;
	vData getProperty(const std::wstring& name) const override;
	virtual bool callMethod(const std::wstring& methodName, const std::vector<vData>& args);
protected:
    HMENU m_handle;
    std::vector<MenuItem> m_menuItems;
};

#endif // VMENU_HPP