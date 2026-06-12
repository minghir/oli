#ifndef XWINDOW_HPP
#define XWINDOW_HPP

#pragma once
#include <gtk/gtk.h>      // API-ul nativ GTK 3 / 4
#include <string>
#include "xContainer.hpp" // xWindow moștenește de la xContainer
#include "../../ConsoleManager.hpp"

enum class DbDialogMode { Insert, Update, Delete, View };

enum class WindowType {
    StandardWindow,
    DialogWindow,
    ToolWindow,
    PopupWindow
};

class xMenu; // Forward declaration pentru meniul GTK

class xWindow : public xContainer {
public:
    // Constructor adaptat. Am scos HINSTANCE pentru că nu există în GTK.
    explicit xWindow(const std::string& id, WindowType type, bool isMainWindow, EventDispatcher& dispatcher);

    // Destructor. GTK folosește un sistem de referințe, widget-urile se distrug curat.
    virtual ~xWindow();

    // Creează fereastra GTK reală.
    // Am păstrat semnătura similară pentru compatibilitatea cu plugin-ul, înlocuind tipurile Win32.
    bool create(const std::wstring& className, const std::wstring& title,
        unsigned int style, int x, int y, int w, int h,
        GtkWidget* parent = nullptr);

    // Implementarea metodei virtuale pure din xControl
    void create(GtkWidget* parent) override {
        ConsoleManager::getInstance().log(L"[xWindow::create] Apelată metoda virtuală implicită.");
        this->create(L"XAppDefaultClass", L"Fereastră Implicită", 0, 100, 100, 800, 600, parent);
    }

    // Setează dacă această fereastră oprește gtk_main la închidere
    void setIsMainWindow(bool isMainWindow) { m_isMainWindow = isMainWindow; }
    bool isMainWindow() const { return m_isMainWindow; }

    // 🔥 Adus la zi: În loc de handleMessage(Win32), în GTK ne conectăm la semnale discrete!
    // Păstrăm totuși o metodă virtuală de rutare dacă xContainer o cere.
    void onSignal(const std::string& signalName);

    // Metode de control vizual (Sunt de 10 ori mai simple în GTK!)
    void setMenu(xMenu* pMenu);
    void scaleChildren(int newDpi); // În GTK scalarea e nativă, dar o lăsăm pentru compatibilitate

    void show() {
        if (!m_widget) {
            LOG_ERROR(L"[xWindow::show] Fereastra nu a fost creată încă!");
            return;
        }
        
        // În GTK, show_all afișează fereastra și toți copiii ei dintr-o singură mișcare
        gtk_widget_show_all(m_widget);
        
        if (m_isModal) {
            gtk_window_present(GTK_WINDOW(m_widget));
        }
    }

    void showModal();
    virtual void hide();
    void close();
    bool isVisible() const;
    void centerWindow();
    void setIcon(const std::wstring& iconPath);

    bool isModal() const { return m_isModal; }
    
    bool setProperty(const std::wstring& name, const vData& value) override;
    vData getProperty(const std::wstring& name) const override;

protected:
    bool m_isMainWindow;
    WindowType m_WindowType;
    bool m_isModal = false;
    GtkWidget* m_gParentForModal = nullptr; // Înlocuitorul HWND m_hParentForModal
};

#endif // XWINDOW_HPP