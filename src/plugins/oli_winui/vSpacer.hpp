#ifndef VSPACER_HPP
#define VSPACER_HPP

#include "vControl.hpp"

class vSpacer : public vControl {
private :
    //HINSTANCE m_hInstance;
public:
    static bool s_debugMode;

    // Constructorul folosește dispatcher-ul părintelui, deși probabil nu va emite evenimente
    vSpacer( const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher)
        : vControl(nullptr, id, x, y, width, height, dispatcher) {
        m_handle = nullptr; // Ne asigurăm că este null
    }

    // Implementarea metodei pur virtuale: Nu creăm fereastră WinAPI
    void create(HWND parent) override {
        
        if (s_debugMode) {
            // Luăm instanța de la părinte
            m_hInstance = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);

            m_handle = CreateWindowExW(
                0,
                L"STATIC",
                nullptr, // Garantează că nu apare text rezidual
                WS_CHILD | WS_VISIBLE | WS_BORDER | SS_NOTIFY, // SS_NOTIFY e opțional
                m_x, m_y, m_width, m_height,
                parent,
                (HMENU)(INT_PTR)getWin32Id(),
                m_hInstance,
                nullptr
            );

            // Dacă vrei să fie și mai vizibil la debug, îi poți da un text scurt:
            // SetWindowTextW(m_handle, L"[Spacer]");
        }
        else {
            m_handle = nullptr;
        }
    }

    // Suprascriem resize/moveAndResize pentru a preveni apelurile WinAPI pe un handle NULL
    void moveAndResize(int x, int y, int width, int height) override {
        m_x = x; m_y = y; m_width = width; m_height = height;
        if (m_handle && s_debugMode) {
            SetWindowPos(m_handle, nullptr, m_x, m_y, m_width, m_height, SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    void resize() override {
        // Dacă spacer-ul are copii (puțin probabil), îi redimensionăm
        for (auto& pair : m_children) {
            pair.second->resize();
        }
    }

    bool isSpacer() const override { return true; }
};

#endif