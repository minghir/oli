#ifndef VRICHEDIT_HPP
#define VRICHEDIT_HPP
#pragma once

#include "vControl.hpp"
#include <richedit.h>
#include <richole.h>
#include <string>

class vRichEdit : public vControl {
private:
    static HMODULE s_richEditModule;
    bool m_isReadOnly = false;
    HFONT m_activeFont = nullptr;

    bool m_isResizable = false;
    bool m_isResizing = false;
    int m_resizeMargin = 5;

    // 🔥 TREBUIE ADĂUGATE ACESTEA:
    int m_lastMouseY = 0;           // Reține poziția Y pe ecran la fiecare mișcare
    int m_resizeDelta = 0;          // Stochează delta pentru a fi citit din script

public:
    vRichEdit(HINSTANCE hInstance, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher);
    ~vRichEdit();

    friend LRESULT CALLBACK RichEditTabSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    void create(HWND parent) override;
    void setTextColorRange(int start, int end, COLORREF color, bool bold = false);
    void setText(const std::wstring& text);
    std::wstring getText() const;
    void setReadOnly(bool readOnly);
    void freeze();
    void unfreeze();

    void setFont(const std::wstring& fontName, int baseFontSize, int weight, bool italic, bool underline) override;
    void setFontSize(int baseFontSize) override;
    void scaleFont(int newDpi) override;

    HFONT getActiveFont() const { return m_activeFont; }

    bool setProperty(const std::wstring& name, const vData& value) override;
    vData getProperty(const std::wstring& name) const override;
    bool callMethod(const std::wstring& methodName, const std::vector<vData>& args) override;

    void appendText(const std::wstring& text);
    void setResizable(bool resizable) { m_isResizable = resizable; }
};
#endif