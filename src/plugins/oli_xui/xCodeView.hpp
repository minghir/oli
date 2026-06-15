#ifndef XCODEVIEW_HPP
#define XCODEVIEW_HPP

#pragma once
#include "xPanel.hpp"
#include <gtk/gtk.h>
#include <string>
#include <vector>

class xCodeView : public xPanel {
private:
    GtkWidget* m_scrolledWindow = nullptr;
    GtkWidget* m_textView = nullptr;
    
    std::wstring m_currentFilePath;
    std::wstring m_syntaxPath;
    int m_fontSize = 12;
    
    bool m_isDirty = false;       // Flag-ul de urmărire a modificărilor
    bool m_ignoreChange = false;  // Protecție la încărcarea fișierului

public:
    explicit xCodeView(
        const std::string& id,
        int x, int y, int width, int height,
        EventDispatcher& dispatcher
    );

    virtual ~xCodeView() = default;

    void create(GtkWidget* parent) override;

    void setText(const std::wstring& text);
    std::wstring getText() const;
    void setFontSize(int size);
    
    void gotoLine(int lineNum);
    void showGoToLineDialog();

    bool setProperty(const std::wstring& name, const vData& value) override;
    vData getProperty(const std::wstring& name) const override;
    bool callMethod(const std::wstring& methodName, const std::vector<vData>& args) override;

    // Permitem callback-ului nativ de GTK să modifice starea internă defensiv
    void handleTextChanged();
    bool shouldIgnoreChange() const { return m_ignoreChange; }
};

#endif // XCODEVIEW_HPP