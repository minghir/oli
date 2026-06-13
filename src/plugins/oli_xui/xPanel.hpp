#ifndef XPANEL_HPP
#define XPANEL_HPP

#pragma once
#include "xContainer.hpp"
#include "../../ConsoleManager.hpp"
#include <gtk/gtk.h>

// xPanel în GTK este de obicei un GtkBox, GtkGrid sau GtkFixed 
// care poate conține alte widget-uri.
class xPanel : public xContainer {
protected:
    bool m_scrollBarOn = false;

public:
    // Constructorul nu mai are nevoie de HINSTANCE
    explicit xPanel(
        const std::string& id,
        int x, int y, int width, int height,
        EventDispatcher& dispatcher
    );

    virtual ~xPanel() = default;

    // Metoda create acum folosește GtkWidget* pentru parent
    void create(GtkWidget* parent) override;

    // În GTK, mesajele sunt semnale. Nu mai avem LRESULT handleMessage.
    // Vom folosi connectSignal pentru evenimente.

    void setScrollBarOn(bool on) { m_scrollBarOn = on; }

    void scale(int newDpi) override {
        xContainer::scale(newDpi);
        // Poți adăuga aici logică specifică de redimensionare font/padding GTK
    }
	
    bool setProperty(const std::wstring& name, const vData& value) override;
    vData getProperty(const std::wstring& name) const override;
    bool callMethod(const std::wstring& methodName, const std::vector<vData>& args) override;
    
    // Metoda de redimensionare proprie GTK
    void moveAndResize(int x, int y, int width, int height);

protected:
    void onClick() override;
    void onMouseClick(int x, int y);

private:
    bool m_isPressed;
    
    // În GTK nu avem nevoie de registerPanelClass (atom), 
    // deoarece widget-urile sunt create prin apeluri de funcții Gtk.
    void updateLayout();
};

#endif // XPANEL_HPP