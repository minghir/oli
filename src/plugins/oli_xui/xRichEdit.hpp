#ifndef XRICHEDIT_HPP
#define XRICHEDIT_HPP

#pragma once
#include "xControl.hpp"
#include <gtk/gtk.h>
#include <string>
#include <vector>

class xRichEdit : public xControl {
private:
    GtkWidget* m_textView = nullptr;
    bool m_isReadOnly = false;
    bool m_isResizable = false;
    bool m_isResizing = false;
    int m_resizeMargin = 8; // Zona de sensibilitate în pixeli pentru drag

    int m_lastMouseY = 0;
    int m_resizeDelta = 0;

public:
    explicit xRichEdit(
        const std::string& id, 
        int x, int y, int width, int height, 
        EventDispatcher& dispatcher
    );
    
    virtual ~xRichEdit() = default;

    void create(GtkWidget* parent) override;
    
    void setText(const std::wstring& text);
    std::wstring getText() const;
    void appendText(const std::wstring& text);
    void setReadOnly(bool readOnly);
    void setFontSize(int size);

    bool setProperty(const std::wstring& name, const vData& value) override;
    vData getProperty(const std::wstring& name) const override;
    bool callMethod(const std::wstring& methodName, const std::vector<vData>& args) override;

    // Callbacks pentru evenimentele de mouse (Resizing)
    void handleButtonPress(GdkEventButton* event);
    void handleButtonRelease(GdkEventButton* event);
    void handleMotionNotify(GdkEventMotion* event);
};

#endif // XRICHEDIT_HPP