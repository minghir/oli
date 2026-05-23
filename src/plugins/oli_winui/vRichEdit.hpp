#ifndef VRICHEDIT_HPP
#define VRICHEDIT_HPP

#pragma once

#include "vControl.hpp"
#include <richedit.h>
#include <richole.h>
#include <string>

class vRichEdit : public vControl {
private:
    static HMODULE s_richEditModule; // Avem nevoie de un singur handle pentru DLL
    bool m_isReadOnly = false;

public:
    vRichEdit(HINSTANCE hInstance, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher);
    virtual ~vRichEdit() = default;

    void create(HWND parent) override;

    // Metodă pentru a schimba culoarea unui text la o anumită poziție
    void setTextColorRange(int start, int end, COLORREF color, bool bold = false);

    void setText(const std::wstring& text);
    std::wstring getText() const;

    void setReadOnly(bool readOnly);

    // Utile pentru Syntax Highlighting
    void freeze();   // Oprește redesenarea (pentru performanță în timpul colorării)
    void unfreeze(); // Repornește redesenarea
};

#endif