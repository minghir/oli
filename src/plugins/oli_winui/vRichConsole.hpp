#pragma once
#include "vRichEdit.hpp"
#include "../../ConsoleManager.hpp" // Pentru ILogOutput
#include <richedit.h>                // IMPORTANT: Pentru CHARFORMAT2, CFM_COLOR, SCF_SELECTION etc.

class vRichConsole : public vRichEdit, public ILogOutput {
public:
    vRichConsole(HINSTANCE hInstance, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher)
        : vRichEdit(hInstance, id, x, y, width, height, dispatcher) {
        
        // Înregistrare automată la ConsoleManager
        ConsoleManager::getInstance().addOutput(this);
    }

    ~vRichConsole() {
        // Dezînregistrare automată
        ConsoleManager::getInstance().removeExtraOutput(this);
    }

    // Implementarea interfeței ILogOutput cu suport pentru culori
    void writeLog(const std::wstring& message, LogLevel level) override {
        std::wstring prefix = L"";
        COLORREF color = RGB(220, 220, 220); // Gri deschis / Alb (implicit pentru INFO)

        // Mapăm fiecare nivel la un prefix și o culoare RGB
        switch (level) {
            case LogLevel::DEBUG:
                prefix = L"[DEBUG] ";
                color = RGB(50, 150, 255); // Albastru deschis
                break;
            case LogLevel::INFO:
                prefix = L"[INFO] ";
                color = RGB(140, 140, 140); // Gri deschis
                break;
            case LogLevel::SUCCESS:
                prefix = L"[SUCCESS] ";
                color = RGB(50, 220, 50);   // Verde luminos
                break;
            case LogLevel::WARNING:
                prefix = L"[WARN] ";
                color = RGB(255, 200, 0);  // Galben / Portocaliu
                break;
            case LogLevel::LOG_ERROR:
                prefix = L"[ERROR] ";
                color = RGB(255, 80, 80);   // Roșu deschis
                break;
            case LogLevel::FATAL_ERROR:
                prefix = L"[FATAL] ";
                color = RGB(255, 0, 0);     // Roșu aprins
                break;
        }

        std::wstring fullLine = prefix + message + L"\n";
        
        // Apelăm funcția internă de scriere colorată
        appendColoredText(fullLine, color);
    }

private:
    // Funcție utilitară nativă Win32 pentru adăugarea de text colorat
   void appendColoredText(const std::wstring& text, COLORREF color) {
        // 🔥 FIX 1: Compilatorul ne-a spus clar că se numește m_handle!
        HWND hEdit = this->m_handle; 
        if (!hEdit) return;

        // Mutăm cursorul/selecția la sfârșitul absolut al textului curent
        int len = GetWindowTextLengthW(hEdit);
        SendMessageW(hEdit, EM_SETSEL, len, len);

        CHARFORMAT2W cf;
        ZeroMemory(&cf, sizeof(cf));
        
        // 🔥 FIX 2: Am scos paranteza în plus de la sizeof(cf)
        cf.cbSize = sizeof(cf); 
        
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = color;
        cf.dwEffects &= ~CFE_AUTOCOLOR; // Dezactivăm culoarea automată a sistemului

        // Aplicăm formatarea doar pe selecția curentă (la final)
        SendMessageW(hEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

        // Inserăm textul (va prelua culoarea setată)
        SendMessageW(hEdit, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());

        // Auto-scroll la final când apar loguri noi
        SendMessageW(hEdit, WM_VSCROLL, SB_BOTTOM, 0);
    }
};