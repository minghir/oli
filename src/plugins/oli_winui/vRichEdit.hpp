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

	HFONT m_activeFont = nullptr; // Stocăm fontul aici
	
	bool m_isResizable = false;     // Flag-ul dorit
    bool m_isResizing = false;      // Stare internă (mouse-ul este apăsat)
    int m_resizeMargin = 5;         // Zona sensibilă la click (px)
	
public:
    vRichEdit(HINSTANCE hInstance, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher);
    ~vRichEdit();
	
	friend LRESULT CALLBACK RichEditTabSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	
    void create(HWND parent) override;

    // Metodă pentru a schimba culoarea unui text la o anumită poziție
    void setTextColorRange(int start, int end, COLORREF color, bool bold = false);

    void setText(const std::wstring& text);
    std::wstring getText() const;

    void setReadOnly(bool readOnly);

    // Utile pentru Syntax Highlighting
    void freeze();   // Oprește redesenarea (pentru performanță în timpul colorării)
    void unfreeze(); // Repornește redesenarea

	void setFont(const std::wstring& fontName, int baseFontSize, int weight, bool italic, bool underline) override;
    void setFontSize(int baseFontSize) override;
    void scaleFont(int newDpi) override; // FOARTE IMPORTANT pentru scalare
	
	HFONT getActiveFont() const { return m_activeFont; }
	
	bool setProperty(const std::wstring& name, const vData& value) override;
	vData getProperty(const std::wstring& name) const override;
	bool callMethod(const std::wstring& methodName, const std::vector<vData>& args) override;

	void appendText(const std::wstring& text); // Helper pentru consolă
	
	void setResizable(bool resizable) { m_isResizable = resizable; }
	
};

#endif