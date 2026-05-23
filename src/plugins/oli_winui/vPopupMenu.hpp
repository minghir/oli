#ifndef VPOPUPMENU_HPP
#define VPOPUPMENU_HPP

#pragma once
#include "vMenu.hpp"
#include <windowsx.h> // Pentru GET_X_LPARAM

class vPopupMenu : public vMenu {
public:
    vPopupMenu(const std::string& id, EventDispatcher& dispatcher)
        : vMenu(id, dispatcher) {}

    // Suprascriem create pentru a folosi CreatePopupMenu
    /*
    void create(HWND parent = nullptr) override {
        if (m_handle) {
            DestroyMenu(m_handle);
        }

        m_handle = CreatePopupMenu();

        // Dacă ai deja iteme în vector (adăugate înainte de create)
        // trebuie să le adăugăm acum în noul handle
        for (const auto& item : m_menuItems) {
            if (item.isSeparator) {
                AppendMenuW(m_handle, MF_SEPARATOR, 0, NULL);
            }
            else {
                AppendMenuW(m_handle, MF_STRING, item.win32Id, item.text.c_str());
            }
        }
    }
    */

    void create(HWND parent = nullptr) override {
        if (m_handle) {
            DestroyMenu(m_handle);
        }

        m_handle = CreatePopupMenu();

        for (const auto& item : m_menuItems) {
            if (item.isSeparator) {
                AppendMenuW(m_handle, MF_SEPARATOR, 0, NULL);
            }
            else {
                // ADAUGĂM VERIFICAREA DE ENABLED AICI:
                UINT flags = MF_STRING;
                if (!item.enabled) {
                    flags |= (MF_DISABLED | MF_GRAYED);
                }

                AppendMenuW(m_handle, flags, item.win32Id, item.text.c_str());
            }
        }
    }

    // Metodă utilitară pentru a afișa meniul direct
    int display(HWND hOwner, int x, int y) {
        if (!getHandle()) return 0;

        return TrackPopupMenu(
            getHandle(),
            TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON,
            x, y,
            0,
            hOwner,
            NULL
        );
    }
};

#endif