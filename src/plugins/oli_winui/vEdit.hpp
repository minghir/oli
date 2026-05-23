// vEdit.hpp
#ifndef VEDIT_HPP
#define VEDIT_HPP

#pragma once

#include "vControl.hpp"
#include <string>
#include <regex>

enum class EditType {
    SINGLE_LINE,
    MULTI_LINE,
    CONSOLE_LINE,
    PASSWORD
};

class vEdit : public vControl {
private: 
    EditType m_editType;
    //std::wstring m_validationRegex;
    //std::wstring m_validationError;
    //bool m_isValid = true;
    bool m_isReadOnly = false;
    bool m_onlyDigits = false;
public:
    // Constructor
    // hInstance: Handle-ul instanței aplicației.
    // id: ID-ul intern al controlului.
    // x, y, width, height: Poziția și dimensiunile controlului.
    explicit vEdit(HINSTANCE hInstance, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher, EditType type = EditType::SINGLE_LINE);
          
    explicit vEdit(HINSTANCE hInstance, const std::string& id, EventDispatcher& dispatcher)
        : vEdit(hInstance, id, 0, 0, 100, 30, dispatcher, EditType::SINGLE_LINE) {}
    // Destructor
    virtual ~vEdit() = default;

    void setOnlyDigits(bool enable) {
        m_onlyDigits = enable;
        if (m_handle && enable) {
            // Putem aplica stilul și după creare
            long style = GetWindowLong(m_handle, GWL_STYLE);
            SetWindowLong(m_handle, GWL_STYLE, style | ES_NUMBER);
        }
    }
    bool isNumericOnly() const { return m_onlyDigits; }

    // Suprascrie metoda `create` pentru a crea controlul WinAPI real (EDIT).
    void create(HWND parent) override;

    // Suprascrie `handleMessage` pentru a gestiona mesaje specifice.
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    // Metodă pentru a schimba textul controlului.
    void setText(const std::wstring& newText);

    // Metodă pentru a obține textul curent din control.
    std::wstring getText() const;

    //void onKillFocus() override;

    void scale(int newDpi) override {
        vControl::scale(newDpi);   // Actualizează DPI
        this->scaleFont(newDpi);   // Setează fontul standard (WM_SETFONT pe m_handle)
    }

    EditType getEditType() { return m_editType; }

    void setReadOnly(bool bReadOnly);

    void setValidation(const std::wstring& pattern, const std::wstring& errorMsg) {
        m_validationRegex = pattern;
        m_validationError = errorMsg;
    }

    bool validate() {


        if (m_validationRegex.empty()) {
            m_isValid = true;
            return true;
        }

        std::wregex reg(m_validationRegex);
        std::wstring text = this->getText(); // Presupunând că ai metoda getText()

        m_isValid = std::regex_match(text, reg);

       
        // Feedback vizual automat
        if (!m_isValid) {
            this->setBackgroundColor(RGB(255, 230, 230)); // Roșiatic pentru eroare
            this->setTooltipText(m_validationError);     // Afișăm eroarea în tooltip
            LOG_WARNING(L"INVALID: Campul: " + str_to_wstr(m_id) + L" ("+ text +L") pentru: " + (m_validationRegex));

        }
        else {
            this->setBackgroundColor(RGB(255, 255, 255)); // Alb pentru succes
            this->setTooltipText(L"");
            LOG_DEBUG(L"OK: Campul: " + str_to_wstr(m_id) + L"(" + text + L") pentru: " + (m_validationRegex));
        }

        return m_isValid;
    }

    //bool isValid() { return m_isValid; }

    //std::wstring getValidationError() { return m_validationError; }

    void onKillFocus() {
        // 1. Executăm validarea internă
        bool ok = validate();

        //m_dispatcher.dispatch("lost_focus", m_id);
        // 2. Notificăm sistemul (Dispatcher-ul)
        // Putem trimite un eveniment special "validation_failed" dacă e cazul
        if (!ok) {
            getEventDispatcher().dispatch("validation_error", m_id);
        }

        // 3. Apelăm și logica de bază de lost_focus
        vControl::onKillFocus();
    }
    void setEditType(EditType type);
    
};

#endif // VEDIT_HPP