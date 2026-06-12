#ifndef XBUTTON_HPP
#define XBUTTON_HPP

#pragma once
#include "xControl.hpp"
#include <gtk/gtk.h>
#include <string>

class xButton : public xControl {
public:
    explicit xButton(const std::string& id, const std::wstring& text, int x, int y, int w, int h, EventDispatcher& dispatcher);
    virtual ~xButton() = default;

    void create(GtkWidget* parent) override;

    bool setProperty(const std::wstring& name, const vData& value) override;
    vData getProperty(const std::wstring& name) const override;

private:
    std::wstring m_text;
};

#endif // XBUTTON_HPP