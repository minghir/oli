#ifndef XTABCONTROL_HPP
#define XTABCONTROL_HPP

#pragma once
#include "xContainer.hpp"
#include <gtk/gtk.h>
#include <string>
#include <vector>

struct xTabPage {
    std::string id;
    GtkWidget* childWidget;
    GtkWidget* tabLabelBox;
    GtkWidget* labelWidget;
};

class xTabControl : public xContainer {
public:
    explicit xTabControl(
        const std::string& id,
        int x, int y, int width, int height,
        EventDispatcher& dispatcher
    );

    virtual ~xTabControl() = default;

    void create(GtkWidget* parent) override;

    bool setProperty(const std::wstring& name, const vData& value) override;
    vData getProperty(const std::wstring& name) const override;
    bool callMethod(const std::wstring& methodName, const std::vector<vData>& args) override;

private:
    std::vector<xTabPage> m_pages;
    
    // Helper pentru localizarea indexului intern după ID-ul paginii
    int findPageIndexById(const std::string& pageId) const;
};

#endif // XTABCONTROL_HPP