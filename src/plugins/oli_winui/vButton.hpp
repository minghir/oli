#ifndef VBUTTON_HPP
#define VBUTTON_HPP

#pragma once

#include "vControl.hpp"       // vButton moștenește direct de la vControl
#include "ConsoleManager.hpp" // Pentru logare (poate fi inclus și prin vControl.hpp)
#include <string>             // Pentru std::string (ID) și std::wstring (text buton)


// Clasa vButton reprezintă un control de tip buton WinAPI.
// Moștenește funcționalitățile de bază ale vControl (ID, handle, evenimente).
// Butoanele sunt controale care declanșează acțiuni la click-uri.
class vButton : public vControl {
protected: 
    std::wstring m_label; // Textul (eticheta) vizibilă pe buton.
public:
    /**
     * @brief Constructor. Inițializează butonul cu un ID intern, textul (eticheta) său,
     * și poziția/dimensiunea sa inițială.
     * @param id ID-ul unic al butonului (string).
     * @param label Textul (eticheta) vizibilă pe buton (wstring pentru compatibilitate WinAPI).
     * @param x Coordonata X a colțului stânga sus al butonului.
     * @param y Coordonata Y a colțului stânga sus al butonului.
     * @param width Lățimea butonului.
     * @param height Înălțimea butonului.
     * @param dispatcher O referință la EventDispatcher-ul central.
     */
    vButton(HINSTANCE hInstance, const std::string& id, const std::wstring& label, int x, int y, int width, int height, EventDispatcher& dispatcher);



    // Destructorul implicit este suficient, deoarece destructorul vControl
    // și unique_ptr-urile gestionează automat curățarea resurselor.
    virtual ~vButton() = default;

    // Suprascrie metoda `create` din vControl pentru a crea controlul WinAPI real al butonului.
    // parent: HWND-ul ferestrei sau controlului părinte în care va fi plasat acest buton.
    void create(HWND parent) override;

    // Suprascrie metoda `handleMessage` din vControl.
    // Această metodă gestionează mesajele WinAPI primite direct de către instanța butonului.
    // De obicei, butoanele procesează mesaje legate de desenare sau stări interne,
    // nu `WM_COMMAND`, deoarece ele *trimit* `WM_COMMAND` către părintele lor.
    // hwnd: Handle-ul butonului care primește mesajul.
    // msg: Codul mesajului WinAPI.
    // wParam, lParam: Parametrii mesajului.
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    // vButton trebuie să aibă o implementare explicită pentru onClick()
    // Aceasta va fi apelată de vContainer/vWindow la primirea WM_COMMAND cu BN_CLICKED.
    void onClick() override;

    void scale(int newDpi) override {
        vControl::scale(newDpi);   // Actualizează DPI
        this->scaleFont(newDpi);   // Setează fontul standard (WM_SETFONT pe m_handle)
    }

    void setText(const std::wstring& text) override;
	
	bool setProperty(const std::wstring& name, const vData& value) override;
    vData getProperty(const std::wstring& name) const override;
private:
    //HINSTANCE m_hInstance;
   

    std::wstring m_iconFilePath;
    bool m_hasIcon = false; // Flag pentru a ști dacă butonul este cu text sau cu pictogramă.

    HWND m_tooltipHandle = nullptr; // Handle-ul controlului tooltip
};

#endif // VBUTTON_HPP