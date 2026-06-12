#ifndef XWINDOW_MANAGER_HPP
#define XWINDOW_MANAGER_HPP

#pragma once
#include "xWindow.hpp" // Managerul gestionează acum obiecte xWindow
#include <gtk/gtk.h>
#include <memory>      // Pentru std::unique_ptr
#include <map>         // Pentru std::map
#include <string>      // Pentru std::string

class XWindowManager {
public:
    XWindowManager() = default;
    ~XWindowManager() = default;

    // Adaugă o fereastră la manager (Preia proprietatea unique_ptr)
    void add(const std::string& id, std::unique_ptr<xWindow> win);

    // Returnează un pointer brut (fără proprietate) către o fereastră după ID-ul său
    xWindow* get(const std::string& id);

    // Elimină o fereastră din manager și o distruge automat
    void remove(const std::string& id);

    // Închide toate ferestrele și golește colecția în siguranță
    void shutdown();

    /**
     * @brief Returnează o fereastră pe baza widget-ului nativ GTK.
     * @param widget Pointerul GtkWidget* căutat.
     */
    xWindow* getWindowByHandle(GtkWidget* widget);

    xWindow* getFirstWindow() {
        if (m_windows.empty()) return nullptr;
        return m_windows.begin()->second.get();
    }

    bool exists(const std::string& id) const {
        return m_windows.find(id) != m_windows.end();
    }

private:
    std::map<std::string, std::unique_ptr<xWindow>> m_windows;
};

// Aliasing de compatibilitate pentru restul proiectului C++
using WindowManager = XWindowManager;

#endif // XWINDOW_MANAGER_HPP