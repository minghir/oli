#ifndef VPANEL_HPP
#define VPANEL_HPP

#pragma once

#include "vContainer.hpp"
#include "ConsoleManager.hpp"

// Clasa vPanel reprezintă un control de tip panou WinAPI,
// care poate găzdui alte controale.
class vPanel : public vContainer {
protected:
    bool m_scrollBarOn = false;
public:
    // Constructor cu ID și handle de instanță (valori implicite).
  //  explicit vPanel(HINSTANCE hInstance, const std::string& id, EventDispatcher& dispatcher);

    // Noul constructor care acceptă și poziția/dimensiunea.
    explicit vPanel(
        HINSTANCE hInstance,
        const std::string& id,
        int x, int y, int width, int height,
        EventDispatcher& dispatcher
    );

    void setScrollBarOn(bool on) { m_scrollBarOn = on; }
    // Destructor implicit.
    virtual ~vPanel() = default;

    // Suprascrie metoda virtuală pură `create` din vControl
    // pentru a crea controlul WinAPI real al panoului.
    void create(HWND parent) override;

    // Suprascrie `handleMessage` din vContainer pentru a procesa
    // mesaje specifice panoului.
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    // Setează culoarea de fundal a panoului.
   // void setBackgroundColor(COLORREF color);

    void scale(int newDpi) override {
        
        // Apelăm implementarea din vContainer, care conține bucla de propagare la copii
        vContainer::scale(newDpi);
        
    }

protected:
    // S-a mutat m_hInstance aici pentru a fi accesibil claselor derivate.
    //HINSTANCE m_hInstance;

    // Suprascrie `onClick` pentru a adăuga logică specifică panoului.
    void onClick() override;

    // Gestionează clicurile de mouse cu coordonate.
    void onMouseClick(int x, int y);

private:
    //COLORREF m_backgroundColor;
    bool m_isPressed;

    // Metodă statică pentru a înregistra clasa de fereastră a panoului.
    static ATOM registerPanelClass(HINSTANCE hInstance);
    static ATOM s_panelClassAtom;
};

#endif // VPANEL_HPP