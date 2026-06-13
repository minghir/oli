#ifndef XCONTROL_HPP
#define XCONTROL_HPP

#pragma once

#include <gtk/gtk.h>       // Include-ul universal pentru GTK 3 / 4
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

#include "EventDispatcher.hpp"
#include "../../ConsoleManager.hpp"
#include "../../StringUtils.hpp"
#include "../../OliEngine.hpp"
#include "../../IOliEngine.hpp"

enum class ControlType {
    Window, Panel, Button, Label, Edit, Checkbox, Combobox,
    ListView, TabControl, Unknown, RadioButton, DatePicker,
    Separator, StatusBar, RadioGroup
};

enum class SizeMode { FIXED, FILL, AUTO, PERCENT };

enum class TextAlign {
    LEFT = 1 << 0, CENTER = 1 << 1, RIGHT = 1 << 2,
    TOP = 1 << 3, MIDDLE = 1 << 4, BOTTOM = 1 << 5
};

inline TextAlign operator|(TextAlign a, TextAlign b) { return static_cast<TextAlign>(static_cast<int>(a) | static_cast<int>(b)); }
inline TextAlign operator&(TextAlign a, TextAlign b) { return static_cast<TextAlign>(static_cast<int>(a) & static_cast<int>(b)); }
inline bool hasFlag(TextAlign value, TextAlign flag) { return (static_cast<int>(value) & static_cast<int>(flag)) != 0; }

enum class Anchor {
    NONE = 0, LEFT = 1 << 0, RIGHT = 1 << 1, TOP = 1 << 2, BOTTOM = 1 << 3,
    CENTER_H = 1 << 4, CENTER_V = 1 << 5, CENTER = (1 << 4) | (1 << 5)
};

inline Anchor operator|(Anchor a, Anchor b) { return static_cast<Anchor>(static_cast<int>(a) | static_cast<int>(b)); }
inline Anchor operator&(Anchor a, Anchor b) { return static_cast<Anchor>(static_cast<int>(a) & static_cast<int>(b)); }
inline bool hasFlag(Anchor value, Anchor flag) { return (static_cast<int>(value) & static_cast<int>(flag)) != 0; }

// =================================================================
// CLASA DE BAZĂ ADAPTATĂ: xControl
// =================================================================
class xControl {
public:
    // S-a eliminat HINSTANCE de la constructori (nu există conceptul în GTK)
    explicit xControl(const std::string& id, EventDispatcher& dispatcher);
    explicit xControl(const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher);
    
    virtual ~xControl();

    void setParent(xControl* parent) { m_parent = parent; }
    xControl* getParent() const { return m_parent; }

    const std::string& getId() const { return m_id; }
    bool setId(const std::string& newId);

    // Returnează handle-ul nativ GTK (GtkWidget*)
    GtkWidget* getHandle() const { return m_widget; }

    // Metodă virtuală pură adaptată pentru ierarhia GTK
    virtual void create(GtkWidget* parent);
    virtual void resize();

    virtual void show();
    virtual void hide();
    bool isVisible() const;
    bool isLogicVisible() const { return m_logicVisible; }

    // Structura m_children a fost păstrată 1:1 ca să nu strici iteratoarele din VM
    void addChild(const std::string& id, std::unique_ptr<xControl> ctrl);
    void addChild(const std::string& id, std::unique_ptr<xControl> ctrl, GtkWidget* visualParent);
    xControl* addChildWithReturn(const std::string& id, std::unique_ptr<xControl> ctrl);
    xControl* getChild(const std::string& id);
    void removeChild(const std::string& id);
    std::unique_ptr<xControl> releaseChild(const std::string& id);

    const std::vector<std::pair<std::string, std::unique_ptr<xControl>>>& getChildren() const { return m_children; }

    // 🔥 Adus la zi: În GTK nu mai avem nevoie de proceduri statice de ferestre sau mesaje brute!
    // Mesajele Win32 sunt înlocuite de conexiuni flexibile de semnale direct în clasele derivate.
    virtual void onClick();
    
    void on(const std::string& eventName, EventDispatcher::EventCallback callback) {
        m_dispatcher.registerHandler(eventName, m_id, callback);
    }
    void on(const std::string& eventName, EventDispatcher::EventCallbackWithArg callback) {
        m_dispatcher.registerHandler(eventName, m_id, callback);
    }

    xControl* getChildRecursive(const std::string& id);
    xControl* findControlByHandle(GtkWidget* widget);

    virtual void onKillFocus() {}
    void setTooltipText(const std::wstring& text);

    // Coordonate și Geometrie
    void setRect(int x, int y, int w, int h) { m_base_x = x; m_base_y = y; m_base_width = w; m_base_height = h; }
    int getX() const { return m_x; }
    int getY() const { return m_y; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

    void setBaseWidth(int w) { m_base_width = w; }
    void setBaseHeight(int h) { m_base_height = h; }
    void setHeight(int h) { m_height = h; if (m_widget) gtk_widget_set_size_request(m_widget, m_width, m_height); }
    void setWidth(int w) { m_width = w; if (m_widget) gtk_widget_set_size_request(m_widget, m_width, m_height); }
    void setBaseX(int x) { m_base_x = x; }
    void setBaseY(int y) { m_base_y = y; }
    void setX(int x) { m_x = x; }
    void setY(int y) { m_y = y; }
    
    int getBaseX() const { return m_base_x; }
    int getBaseY() const { return m_base_y; }
    int getBaseWidth() const { return m_base_width; }
    int getBaseHeight() const { return m_base_height; }

    void setMinSize(int w, int h);
    void setMaxSize(int w, int h);
    int getMinWidth() const { return m_minWidth; }
    int getMinHeight() const { return m_minHeight; }

    // Management Fonturi (Folosește subsistemul GTK Pango)
    virtual void setFont(const std::wstring& fontName, int baseFontSize, int weight = 400, bool italic = false, bool underline = false);
    virtual void setFontName(const std::wstring& fontName);
    virtual void setFontSize(int baseFontSize);
    const std::wstring getFontName() const { return m_fontName; }
    int getFontSize() const { return m_baseFontSize; }

    // Layout configuration
    void setAnchor(Anchor a) { anchor = a; }
    void setWidthMode(SizeMode mode) { widthMode = mode; }
    void setHeightMode(SizeMode mode) { heightMode = mode; }
    Anchor getAnchor() const { return anchor; }
    SizeMode getWidthMode() const { return widthMode; }
    SizeMode getHeightMode() const { return heightMode; }

    void setMargins(int left, int top, int right, int bottom) {
        marginLeft = left; marginTop = top; marginRight = right; marginBottom = bottom;
    }
    int getMarginLeft() const { return marginLeft; }
    int getMarginTop() const { return marginTop; }
    int getMarginRight() const { return marginRight; }
    int getMarginBottom() const { return marginBottom; }

    virtual void setText(const std::wstring& text);
    virtual std::wstring getText() const { return L""; }
    void update() { /* În GTK redesenarea se face automat thread-safe */ }
    virtual void moveAndResize(int x, int y, int width, int height);

    // Grid behavior
    void setGridPosition(int row, int col) { m_gridRow = row; m_gridColumn = col; }
    int getGridRow() const { return m_gridRow; }
    int getGridColumn() const { return m_gridColumn; }

    ControlType getType() const { return m_ControlType; }
    EventDispatcher& getEventDispatcher() const { return m_dispatcher; }
    virtual void clearChildren();

    void setOnClick(std::function<void()> callback) { m_onClickCallback = callback; }

    // Helperii tăi de șabloane rămân identici (Zero modificări în logica ta de căutare!)
    template <typename T> T* getChildAs(const std::string& name) { return dynamic_cast<T*>(getChild(name)); }
    template <typename T> T* findChild(const std::string& id) {
        xControl* ctrl = getChildRecursive(id);
        return ctrl ? dynamic_cast<T*>(ctrl) : nullptr;
    }

    // 🔥 Stilizarea Nativă GTK prin CSS Providers (Înlocuitorii COLORREF)
    virtual void setBackgroundColor(const std::string& cssColor);
    virtual void setTextColor(const std::string& cssColor);
    void setEnabled(bool enable);
    bool isEnabled() const { return m_enabled; }

    // Sistem Generic de Atribute din script
    void setAttribute(const std::wstring& key, const std::wstring& value) { m_attributes[key] = value; }
    std::wstring getAttribute(const std::wstring& key, const std::wstring& defaultValue = L"") const {
        auto it = m_attributes.find(key); return (it != m_attributes.end()) ? it->second : defaultValue;
    }
    bool hasAttribute(const std::wstring& key) const { return m_attributes.find(key) != m_attributes.end(); }
    void removeAttribute(const std::wstring& key) { m_attributes.erase(key); }

    // Logica de validare din scripturi
    virtual bool validate() { m_isValid = true; return true; }
    bool isValid() const { return m_isValid; }
    void setValidation(const std::wstring& pattern, const std::wstring& errorMsg) { m_validationRegex = pattern; m_validationError = errorMsg; }
    bool validateRecursive();
    std::wstring getValidationError() const { return m_validationError; }
	
    // Extensibilitatea către Mașina Virtuală Oli
    virtual bool setProperty(const std::wstring& name, const vData& value);
    virtual vData getProperty(const std::wstring& name) const;
    virtual bool callMethod(const std::wstring& /*methodName*/, const std::vector<vData>& /*args*/) { return false; }
	
    void updateSize(int newWidth, int newHeight) {
        m_width = newWidth; m_height = newHeight;
        if (m_widget) gtk_widget_set_size_request(m_widget, m_width, m_height);
    }

    
	
protected:
    std::function<void()> m_onClickCallback = nullptr;
    GtkWidget* m_widget = nullptr;        // Inlocuitorul direct al HWND m_handle
    bool m_enabled = true;
    std::string m_id;
    ControlType m_ControlType = ControlType::Unknown;

    std::vector<std::pair<std::string, std::unique_ptr<xControl>>> m_children;

    SizeMode widthMode = SizeMode::FIXED;
    SizeMode heightMode = SizeMode::FIXED;
    Anchor anchor = Anchor::LEFT | Anchor::TOP;

    int marginLeft = 0, marginTop = 0, marginRight = 0, marginBottom = 0;
    int m_x = 0, m_y = 0, m_width = 0, m_height = 0;
    int m_base_x = 0, m_base_y = 0, m_base_width = 0, m_base_height = 0;
    int m_minWidth = 0, m_minHeight = 0, m_maxWidth = 32767, m_maxHeight = 32767;

    EventDispatcher& m_dispatcher;
    xControl* m_parent = nullptr;

    // Proprietățile de stilizare salvate pentru interogările VM-ului
    std::wstring m_fontName;
    int m_baseFontSize = 10;
    int m_fontWeight = 400;
    bool m_fontItalic = false;
    bool m_fontUnderline = false;
    PangoFontDescription* m_pangoFont = nullptr; // Inlocuitorul HFONT m_hFont

    int m_gridRow = 0, m_gridColumn = 0;
    std::string m_bgColorCss;
    std::string m_textColorCss;

    std::map<std::wstring, std::wstring> m_attributes;
    bool m_isValid = true;
    std::wstring m_validationRegex;
    std::wstring m_validationError;
    bool m_logicVisible = true;
};

#endif // XCONTROL_HPP