#ifndef VWINDOW_HPP
#define VWINDOW_HPP

#pragma once

#include <windows.h>      // Declarații WinAPI
#include <string>         // Pentru std::string și std::wstring

#include "vContainer.hpp" // vWindow moștenește de la vContainer
#include "ConsoleManager.hpp" // Pentru logare

enum class DbDialogMode { Insert, Update, Delete, View };


enum class WindowType {
    StandardWindow,
    DialogWindow,
    ToolWindow,
    PopupWindow
};

class vMenu; // Forward declaration

// Clasa vWindow reprezintă o fereastră WinAPI.
// Moștenește funcționalitățile de bază ale vControl (ID, handle, copii, evenimente)
// și pe cele de container ale vContainer (dispecerizare WM_COMMAND către copii).
// Această clasă se ocupă în mod specific de înregistrarea clasei de fereastră
// și de crearea ferestrei de nivel superior (sau sub-fereastră).
class vWindow : public vContainer {
public:
    // Constructor. Inițializează fereastra cu un handle de instanță WinAPI, un ID intern
    // și un flag care indică dacă este fereastra principală a aplicației.
    //explicit vWindow(HINSTANCE hInstance, const std::string& id, EventDispatcher& dispatcher, bool isMainWindow = false);

    explicit vWindow(HINSTANCE hInstance, const std::string& id, WindowType type, bool isMainWindow, EventDispatcher& dispatcher);


    // Destructor virtual. Se bazează pe destructorul vControl pentru a curăța HWND-ul.
    //virtual ~vWindow() = default;
    ~vWindow();

    // Creează fereastra WinAPI reală.
    // Aceasta este metoda principală de creare pentru o fereastră de nivel superior.
    // className: Numele clasei de fereastră WinAPI.
    // title: Titlul ferestrei.
    // style: Stilurile ferestrei (ex: WS_OVERLAPPEDWINDOW).
    // x, y, w, h: Poziția și dimensiunile ferestrei.
    // parent: HWND-ul ferestrei părinte (nullptr pentru o fereastră top-level).
    // menu: Handle-ul meniului ferestrei.
    bool create(const std::wstring& className, const std::wstring& title,
        DWORD style, int x, int y, int w, int h,
        //HWND parent = nullptr, HMENU menu = nullptr, WNDPROC wndProc = StaticWndProc);
        HWND parent = nullptr, HMENU menu = nullptr);

    // Implementarea metodei virtuale pure `create(HWND parent)` din vControl.
    // Această metodă este destinată controalelor copil. Deoarece o vWindow este de obicei
    // o fereastră de nivel superior sau un container principal, apelarea acestei metode
    // direct pentru o vWindow ar putea fi un semn de utilizare incorectă.
    // Am păstrat o implementare implicită pentru compatibilitate cu baza ta de cod,
    // dar se recomandă utilizarea `create(className, title, ...)` pentru vWindow.
    void create(HWND parent) override {
        ConsoleManager::getInstance().log(L"[vWindow::create(HWND)] Apelată metoda virtuală create cu părinte. Atenție: Se va folosi o creare implicită pentru fereastră.");
        // Apel la metoda detaliată cu valori implicite.
        this->create(L"VAppDefaultWindowClass", L"Fereastră Implicită", // Clasa ar trebui să fie unică
            WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, parent, nullptr);
    }

    // Returnează handle-ul instanței WinAPI (hInstance) asociat cu această fereastră.
    HINSTANCE getInstance() const { return m_hInstance; }

    // Setează dacă această fereastră este fereastra principală a aplicației.
    // Util pentru a controla când PostQuitMessage(0) este apelat la închidere.
    void setIsMainWindow(bool isMainWindow) { m_isMainWindow = isMainWindow; }

    // Verifică dacă această fereastră este fereastra principală.
    bool isMainWindow() const { return m_isMainWindow; }

    // Suprascrie metoda `handleMessage` din vContainer.
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    /**
     * @brief Setează un meniu principal pentru fereastră.
     * @param pMenu Pointer la obiectul vMenu care va fi setat.
     */
    void setMenu(vMenu* pMenu);
    void scaleChildren(int newDpi);

    void show(int nCmdShow = SW_SHOW) {

        //LOG_INFO(L"[vWindow:show()] SUNT IN SHOW");
        if (this == nullptr) {
            LOG_ERROR(L"[vWindow:show()] EROARE: 'this' este NULL!");
            return;
        }

        LOG_INFO(L"[vWindow:show()] Handle-ul este: " + std::to_wstring((uintptr_t)m_handle));

        if (m_handle && IsWindow(m_handle)) {
            
            ShowWindow(m_handle, nCmdShow);
            UpdateWindow(m_handle);
            // Opțional: o aducem în față dacă este un dialog de editare
            SetForegroundWindow(m_handle);
            
        }
        else {
            LOG_ERROR(L"[vWindow:show()] Handle INVALID sau fereastra nu a fost creata!");
        }
    }

    void showModal();

    // Ascunde fereastra (fără a o distruge)
    virtual void hide();

    void close() {
        if (m_handle && IsWindow(m_handle)) {
            // Trimitem WM_CLOSE pentru a permite o închidere "curată"
            PostMessage(m_handle, WM_CLOSE, 0, 0);
        }
    }
    // Verifică dacă fereastra este vizibilă
    bool isVisible() const {
        return m_handle && IsWindowVisible(m_handle);
    }

    void centerWindow();

    bool isModal() const { return m_isModal; }
	
	
	bool setProperty(const std::wstring& name, const vData& value) override;
	vData getProperty(const std::wstring& name) const override;
	
protected:
   // HINSTANCE m_hInstance;      // Handle-ul instanței aplicației.
    bool m_isMainWindow;        // Flag pentru a determina dacă este fereastra principală.
    WindowType m_WindowType;
    bool m_isModal = false;
    HWND m_hParentForModal = NULL; // Membru pentru a ține evidența părintelui
};

#endif // VWINDOW_HPP