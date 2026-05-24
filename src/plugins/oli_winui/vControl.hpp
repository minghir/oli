#ifndef VCONTROL_HPP
#define VCONTROL_HPP

#pragma once

#include <windows.h>      // Declarații WinAPI
#include <string>         // Pentru std::string
#include <map>            // Pentru std::map (m_children)
#include <memory>         // Pentru std::unique_ptr
#include <functional>     // Pentru std::function (EventCallback)

#include "EventDispatcher.hpp" // Pentru sistemul de evenimente
#include "ConsoleManager.hpp"
#include "stringUtils.hpp"

enum class ControlType {
    Window,
    Panel,
    Button,
    Label,
    Edit,
    Checkbox,
    Combobox,
    ListView,
    TabControl,
    Unknown,
    RadioButton,
    DatePicker,
    Separator,
    StatusBar,
    RadioGroup
};




enum class SizeMode {
    FIXED,
    FILL,
    AUTO, 
    PERCENT
};

enum class TextAlign {
    LEFT = 1 << 0,
    CENTER = 1 << 1,
    RIGHT = 1 << 2,
    TOP = 1 << 3,
    MIDDLE = 1 << 4,
    BOTTOM = 1 << 5
};


inline TextAlign operator|(TextAlign a, TextAlign b) {
    return static_cast<TextAlign>(
        static_cast<int>(a) | static_cast<int>(b)
        );
}

inline TextAlign operator&(TextAlign a, TextAlign b) {
    return static_cast<TextAlign>(
        static_cast<int>(a) & static_cast<int>(b)
        );
}

inline bool hasFlag(TextAlign value, TextAlign flag) {
    return (static_cast<int>(value) & static_cast<int>(flag)) != 0;
}

inline TextAlign operator~(TextAlign a) {
    return static_cast<TextAlign>(~static_cast<int>(a));
}

enum class Anchor {
    NONE = 0,
    LEFT = 1 << 0,
    RIGHT = 1 << 1,
    TOP = 1 << 2,
    BOTTOM = 1 << 3,

    CENTER_H = 1 << 4, // Centrare Orizontală
    CENTER_V = 1 << 5, // Centrare Verticală
    CENTER = (1 << 4) | (1 << 5) // Ambele
};

inline Anchor operator|(Anchor a, Anchor b) {
    return static_cast<Anchor>(
        static_cast<int>(a) | static_cast<int>(b)
        );
}

inline Anchor operator&(Anchor a, Anchor b) {
    return static_cast<Anchor>(
        static_cast<int>(a) & static_cast<int>(b)
        );
}

inline bool hasFlag(Anchor value, Anchor flag) {
    return (static_cast<int>(value) & static_cast<int>(flag)) != 0;
}





// Clasa de bază abstractă pentru controalele UI WinAPI.
// Oferă funcționalități comune pentru gestionarea handle-urilor (HWND), ID-urilor,
// structurii copil-părinte și a evenimentelor.
class vControl {//: public EventDispatcher { // Corect: mosteneste public EventDispatcher
public:
    // Constructor. Asociază un ID unic controlului.
    // Alocă și un ID Win32 pentru utilizare cu mesaje de comandă.
    explicit vControl(HINSTANCE hInst, const std::string& id, EventDispatcher& dispatcher);
    explicit vControl(HINSTANCE hInst, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher);

    void setParent(vControl* parent) { m_parent = parent; }
    vControl* getParent() const { return m_parent; }

    // Destructor virtual. Se asigură că HWND-ul controlului este distrus.
    virtual ~vControl();

    // Returnează ID-ul intern al controlului.
    const std::string& getId() const;
    bool setId(const std::string& newId);

    // Returnează handle-ul WinAPI al controlului (HWND).
    HWND getHandle() const;

    // Metodă virtuală pură pentru crearea controlului WinAPI real.
    // Trebuie implementată de clasele derivate (ex: vWindow, vPanel, vButton).
    //virtual void create(HWND parent) = 0;
    virtual void create(HWND parent);
    virtual void resize();

    // Afișează sau ascunde controlul.
    // cmdShow specifică starea de afișare (ex: SW_SHOW, SW_HIDE).
    virtual void show(int cmdShow = SW_SHOW);
    virtual void hide();
    bool isVisible() const;
    bool isLogicVisible() const;

    // Adaugă un control copil la acest control.
    // Preia proprietatea asupra unique_ptr-ului copilului.
    void addChild(const std::string& id, std::unique_ptr<vControl> ctrl);
    void addChild(const std::string& id, std::unique_ptr<vControl> ctrl, HWND visualParent);
    vControl* addChildWithReturn(const std::string& id, std::unique_ptr<vControl> ctrl);

    // Returnează un pointer (care nu deține proprietatea) către un control copil.
    // Returnează nullptr dacă copilul nu este găsit.
    vControl* getChild(const std::string& id);

    // Elimină un control copil din colecție.
    void removeChild(const std::string& id);

    // Handlerul de mesaje pentru instanța controlului.
    // Mesajele WinAPI sunt direcționate aici de către StaticWndProc.
    virtual LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Returnează o referință constantă la harta copiilor controlului.
    //const std::map<std::string, std::unique_ptr<vControl>>& getChildren() const;

    const std::vector<std::pair<std::string, std::unique_ptr<vControl>>>& getChildren() const {
        return m_children;
    }

    // Returnează ID-ul numeric Win32 al controlului.
    int getWin32Id() const;

    // Procedura statică de fereastră (WndProc) pentru WinAPI.
    // Acționează ca un punct de intrare global pentru mesajele Windows
    // și le redirecționează către instanța corectă de vControl.
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Metoda virtuală pentru evenimentul de click.
    // Apelată când controlul primește un mesaj de click (ex: BN_CLICKED).
    // Poate fi suprascrisă de clasele derivate pentru comportament specific.
    virtual void onClick(); // Corect, virtual non-pur
    void on(const std::string& eventName, EventDispatcher::EventCallback callback) {
        m_dispatcher.registerHandler(eventName, m_id, callback);
    }
    void on(const std::string& eventName, EventDispatcher::EventCallbackWithArg callback) {
        m_dispatcher.registerHandler(eventName, m_id, callback);
    }

  //  int getHeight() const { return m_height; }
   // int getWidth() const { return m_width; }

    static WNDPROC getOriginalWndProc();

    //using EventCallback = std::function<void()>;
    //void registerHandler(const std::string& eventName, EventCallback callback);
    //bool dispatch(const std::string& eventName);

    virtual vControl* getChildByWin32Id(int win32Id);

    

    virtual void onKillFocus();
    void setTooltipText(const std::wstring& text);

    // A method to scale the control
    int getPrimaryMonitorDpi();

    void setRect(int x, int y, int w, int h) { m_base_x = x; m_base_y = y; m_base_width = w; m_base_height = h; }
    virtual void scale(int dpi);
    int getX() const;
    int getY() const;
    int getWidth() const;
    int getHeight() const;

    void setBaseWidth(int w) { m_base_width = w; }
    void setBaseHeight(int h) { m_base_height = h; }
    void setHeight(int h) { m_height = h; }
    void setWidth(int w) { m_width = w; }

    void setBaseX(int x) { m_base_x = x; }
    void setBaseY(int y) { m_base_y = y; }

    void setX(int x) { m_x = x; }
    void setY(int y) { m_y = y; }


    int getCurrentDpi() const;
    void scaleFont(int newDpi);
    //void setFont(const std::wstring& fontName, int baseFontSize);
    //void setFont(const std::wstring& fontName, int baseFontSize, int weight, bool italic, bool underline);
    virtual void setFont(const std::wstring& fontName,
        int baseFontSize,
        int weight = FW_NORMAL,  // Valoare default
        bool italic = false,     // Valoare default
        bool underline = false); // Valoare default

    virtual void setFont(HFONT hFont);

    void setFontName(const std::wstring& fontName){
        setFont(fontName, m_baseFontSize, m_fontWeight, m_fontItalic, m_fontUnderline);
    }
    void setFontSize(int baseFontSize) {
        setFont(m_fontName, baseFontSize, m_fontWeight, m_fontItalic, m_fontUnderline);
    }
    void setFontWeight(int weight) {
        //LOG_ERROR(L"SetWeight: " + std::to_wstring(weight));
        setFont(m_fontName, m_baseFontSize, weight, m_fontItalic, m_fontUnderline);
    }
    void setFontItalic(bool italic) {
        setFont(m_fontName, m_baseFontSize, m_fontWeight, italic, m_fontUnderline);
    }
    void setFontUnderline(bool underline) {
        setFont(m_fontName, m_baseFontSize, m_fontWeight, m_fontItalic, underline);
    }


    const std::wstring getFontName() {
        return m_fontName;
    }
    const int getFontSize() {
        return m_baseFontSize;
    }
   



    HFONT getFont() const;

    // --- Layout setters ---
    void setAnchor(Anchor a) { anchor = a; }
    void setWidthMode(SizeMode mode) { widthMode = mode; }
    void setHeightMode(SizeMode mode) { heightMode = mode; }

    Anchor getAnchor() const { return anchor; }
    SizeMode getWidthMode() const { return widthMode; }
    SizeMode getHeightMode() const { return heightMode; }

    void setMargins(int left, int top, int right, int bottom) {
        marginLeft = left;
        marginTop = top;
        marginRight = right;
        marginBottom = bottom;
    }

    virtual void setText(const std::wstring& text) {
        if (m_handle) {
            SetWindowTextW(m_handle, text.c_str());
        }
    }



    void update();



    /**
     * @brief Modifică poziția și dimensiunea controlului WinAPI.
     * @param x Noua coordonată X.
     * @param y Noua coordonată Y.
     * @param width Noua lățime.
     * @param height Noua înălțime.
     */
    virtual void moveAndResize(int x, int y, int width, int height);


    //Anchor getAnchor() const { return anchor; }
    //SizeMode getWidthMode() const { return widthMode; }
    //SizeMode getHeightMode() const { return heightMode; }

    int getMarginLeft() const { return marginLeft; }
    int getMarginTop() const { return marginTop; }
    int getMarginRight() const { return marginRight; }
    int getMarginBottom() const { return marginBottom; }

    int getBaseX() const { return m_base_x; }
    int getBaseY() const { return m_base_y; }
    int getBaseWidth() const { return m_base_width; }
    int getBaseHeight() const { return m_base_height; }

    void setMinSize(int w, int h) { m_minWidth = w; m_minHeight = h; }
    void setMaxSize(int w, int h) { m_maxWidth = w; m_maxHeight = h; }

    int getMinWidth() const { return m_minWidth; }
    int getMinHeight() const { return m_minHeight; }

    const RECT& getOriginalClientRect() const { return m_originalClientRect; }
    void setOriginalClientRect(const RECT& rect) { m_originalClientRect = rect; }


    virtual bool isSpacer() const { return false; }

    //gridlayout tools
    void setGridPosition(int row, int col) { m_gridRow = row; m_gridColumn = col; }
    int getGridRow() const { return m_gridRow; }
    int getGridColumn() const { return m_gridColumn; }

    ControlType getType() const { return m_ControlType; }


    EventDispatcher& getEventDispatcher() const;

    virtual void clearChildren();

    void setOnClick(std::function<void()> callback) {
        m_onClickCallback = callback;
    }


    template <typename T>
    T* getChildAs(const std::string& name) {
        return dynamic_cast<T*>(getChild(name));
    }

    vControl* getChildRecursive(const std::string& id);

    template<typename T>
    T* findChild(const std::string& id) {
        vControl* ctrl = getChildRecursive(id); // Folosește metoda de căutare recursivă
        if (!ctrl) return nullptr;

        // Încearcă să convertească pointerul la tipul cerut (T)
        T* casted = dynamic_cast<T*>(ctrl);
        if (!casted) {
            LOG_ERROR(L"Controlul cu ID-ul '" + str_to_wstr(id) + L"' a fost găsit, dar nu este de tipul cerut!");
        }
        return casted;
    }

    virtual void setBackgroundColor(COLORREF color);
    virtual void setTextColor(COLORREF color);

    COLORREF getBackgroundColor() { return m_backgroundColor; }
    COLORREF getTextColor() { return m_textColor; }

    COLORREF getEffectiveTextColor() const;
    HBRUSH getEffectiveBackgroundBrush() const;
    COLORREF getEffectiveBackgroundColor() const;
    HFONT getEffectiveFont() const;

    void setTextAlign(TextAlign align) { m_textAlign = align; if (m_handle) InvalidateRect(m_handle, NULL, TRUE);}

    vControl* findControlByHandle(HWND hwnd);

    void setEnabled(bool enable);

    bool isEnabled() const { return m_enabled; }

    // --- Sistem Generic de Atribute ---

    void setAttribute(const std::wstring& key, const std::wstring& value) {
        m_attributes[key] = value;
    }

    std::wstring getAttribute(const std::wstring& key, const std::wstring& defaultValue = L"") const {
        auto it = m_attributes.find(key);
        if (it != m_attributes.end()) {
            return it->second;
        }
        return defaultValue;
    }

    bool hasAttribute(const std::wstring& key) const {
        return m_attributes.find(key) != m_attributes.end();
    }

    void removeAttribute(const std::wstring& key) {
        m_attributes.erase(key);
    }

    virtual std::wstring getText() const { return L""; }
   
    virtual bool validate() {
        // Implementare default: dacă nu avem regex, e valid
        m_isValid = true;
        return true;
    }

    bool isValid() const { return m_isValid; }

    void setValidation(const std::wstring& pattern, const std::wstring& errorMsg) {
        m_validationRegex = pattern;
        m_validationError = errorMsg;
    }

    bool validateRecursive();

    std::wstring getValidationError() const { return m_validationError; }
protected:
    
    std::function<void()> m_onClickCallback = nullptr;

    RECT m_originalClientRect{};

    

    HINSTANCE m_hInstance;
    HWND m_handle = nullptr;                                  // Handle-ul WinAPI al controlului
    bool m_enabled = true;
    std::string m_id;                               // ID-ul intern al controlului (string)
    ControlType m_ControlType = ControlType::Unknown;

    //std::map<std::string, std::unique_ptr<vControl>> m_children; // Copiii controlului
     std::vector<std::pair<std::string, std::unique_ptr<vControl>>> m_children;
    
    // NOU: Harta pentru căutări rapide bazate pe ID-ul Win32
    std::map<int, vControl*> m_controlsByWin32Id; // <-- Adăugat

    int m_win32Id; // ID-ul unic numeric utilizat de WinAPI (ex: în WM_COMMAND)

    TextAlign m_textAlign = TextAlign::LEFT | TextAlign::TOP; // Default

    SizeMode widthMode = SizeMode::FIXED;
    SizeMode heightMode = SizeMode::FIXED;

    Anchor anchor = Anchor::LEFT | Anchor::TOP;

    int marginLeft = 0;
    int marginTop = 0;
    int marginRight = 0;
    int marginBottom = 0;


    // Fă membrii protejați pentru a fi accesibili claselor derivate
    int m_currentDpi = 96;
    int m_x, m_y, m_width, m_height;
    int m_base_x, m_base_y, m_base_width, m_base_height;

    int m_minWidth = 0;
    int m_minHeight = 0;
    int m_maxWidth = 32767; // Valoarea maximă short în WinAPI
    int m_maxHeight = 32767;


    std::wstring m_tooltipText;

    static WNDPROC s_originalWndProc;

    EventDispatcher& m_dispatcher;
    vControl* m_parent;

    std::wstring m_fontName; // Numele de bază al fontului (ex: "Segoe UI")
    int m_baseFontSize;      // Mărimea fontului la 96 DPI
    int m_fontWeight = FW_NORMAL;
    bool m_fontItalic = false;
    bool m_fontUnderline = false;
    bool m_hasCustomFont = false;

    HFONT m_hFont = nullptr;

    //membri pentru gridlayout
    int m_gridRow = 0;
    int m_gridColumn = 0;

    COLORREF m_backgroundColor = RGB(255, 255, 255); // Culoare default (Light Gray)
    HBRUSH m_bgBrush = nullptr;
    bool m_hasCustomBackground = false;

    COLORREF m_textColor = RGB(0, 0, 0); // Default Negru
    bool m_hasCustomTextColor = false;

    std::map<std::wstring, std::wstring> m_attributes; // Stocare generică pentru metadate
    
    bool m_isValid = true;
    std::wstring m_validationRegex;
    std::wstring m_validationError;

    bool m_logicVisible = true; // Starea setată de tine prin show()/hide()
};

#endif // VCONTROL_HPP