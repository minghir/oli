#ifndef XMENU_HPP
#define XMENU_HPP

#pragma once
#include <gtk/gtk.h>
#include <string>
#include <vector>
#include "xControl.hpp"

struct MenuItem {
    std::string id;
    std::wstring text;
    GtkWidget* widget; // În GTK, fiecare item e un widget
    bool isSeparator;
};

class xMenu : public xControl {
public:
    explicit xMenu(const std::string& id, EventDispatcher& dispatcher);
    virtual ~xMenu() = default;

    // În GTK, meniul trebuie atașat unei ferestre
    void create(GtkWidget* parent) override;

    void addItem(const std::string& id, const std::wstring& text);
    void addSeparator(const std::string& id);
    void addSubMenu(const std::wstring& text, xMenu* subMenu);

    GtkWidget* getHandle() const { return m_menuWidget; }

    bool setProperty(const std::wstring& name, const vData& value) override;
    vData getProperty(const std::wstring& name) const override;
    bool callMethod(const std::wstring& methodName, const std::vector<vData>& args) override;

protected:
    GtkWidget* m_menuWidget; // GtkMenu propriu-zis
    std::vector<MenuItem> m_menuItems;
};

#endif // XMENU_HPP