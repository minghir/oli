#ifndef VLABEL_HPP
#define VLABEL_HPP

#pragma once

#include "vControl.hpp"
#include <string>

class vLabel : public vControl {
public:
    // Constructor
    // hInstance: Handle-ul instanței aplicației.
    // id: ID-ul intern al controlului.
    // text: Textul inițial afișat de label.
    // x, y, width, height: Poziția și dimensiunile label-ului.
    explicit vLabel(HINSTANCE hInstance, const std::string& id, const std::wstring& text, int x, int y, int width, int height, EventDispatcher& dispatcher);

    // --- CONSTRUCTOR SIMPLIFICAT PENTRU XML / FACTORY ---
    explicit vLabel(HINSTANCE hInstance, const std::string& id, EventDispatcher& dispatcher)
        : vLabel(hInstance, id, L"", 0, 0, 100, 25, dispatcher)
    {
        // Acest constructor delegă totul către cel principal cu 8 argumente.
        // L"" este textul inițial, 0,0,100,25 sunt dimensiuni de backup.
    }

    // Destructor
    virtual ~vLabel() = default;

    // Suprascrie metoda `create` pentru a crea controlul WinAPI real (STATIC).
    // Observați că tipul de retur și parametrii sunt identici cu vControl::create.
    void create(HWND parent) override;

    // Suprascrie metoda `handleMessage` pentru a procesa mesaje specifice,
    // deși pentru un label, de obicei nu sunt multe mesaje speciale de gestionat.
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    // Metodă pentru a schimba textul label-ului dinamic
    void setText(const std::wstring& newText);
    std::wstring getText() const;

    void scale(int newDpi) override {
        vControl::scale(newDpi);   // Actualizează DPI
        this->scaleFont(newDpi);   // Setează fontul standard (WM_SETFONT pe m_handle)
    }

    //void setRightAlign(bool rightAlign) { m_rightAlign = rightAlign; }
private:
    //HINSTANCE m_hInstance;
    std::wstring m_text; // Textul afișat de label
  //  bool m_rightAlign = false;
   
};

#endif // VLABEL_HPP