#ifndef VTABCONTROL_HPP
#define VTABCONTROL_HPP

#pragma once

#include "vContainer.hpp"
#include <commctrl.h>
#include <vector>
#include "vPanel.hpp"

struct vTabPage {
    std::wstring title;
    vPanel* panel;
};

class vTabControl : public vContainer {
public:
    explicit vTabControl(
        HINSTANCE hInstance,
        const std::string& id,
        int x, int y, int width, int height,
        EventDispatcher& dispatcher
    );

    virtual ~vTabControl() = default;

    // Metoda create conform stilului vPanel
    void create(HWND parent) override;

    // Mesaje specifice pentru Tab Control
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    // Metode specifice pentru managementul tab-urilor
    void addTabPage(const std::wstring& title, std::unique_ptr<vPanel> page);
    void switchPage(int index);

    void scale(int newDpi) override {
    
        vContainer::scale(newDpi);
        this->scaleFont(newDpi);
     
        // După scalare, forțăm așezarea paginii curente în noul spațiu
        switchPage(getSelectedIndex());
        this->update();
       
    }

    void setBackgroundColor(COLORREF color) {
        m_backgroundColor = color;
        if (m_handle) {
            InvalidateRect(m_handle, nullptr, TRUE);
        }
    }

    int getSelectedIndex() const;
    vPanel* getCurrentPage() const;
    void moveAndResize(int x, int y, int width, int height);
	
	bool setProperty(const std::wstring& name, const vData& value) override;
	vData getProperty(const std::wstring& name) const override;
	bool callMethod(const std::wstring& methodName, const std::vector<vData>& args) override;
	
private:
    std::vector<vTabPage> m_pages;
    RECT getDisplayRect();
	void refresh();
   // COLORREF m_backgroundColor;
};

#endif