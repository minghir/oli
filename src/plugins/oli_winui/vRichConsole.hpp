#pragma once
#include "vRichEdit.hpp"
#include "ConsoleManager.hpp" // Pentru ILogOutput

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

    // Implementarea interfeței ILogOutput
    void writeLog(const std::wstring& message, LogLevel level) override {
        // Opțional: poți adăuga un prefix în funcție de nivel
        std::wstring prefix = L"";
        if (level == LogLevel::LOG_ERROR) prefix = L"[ERROR] ";
        else if (level == LogLevel::WARNING) prefix = L"[WARN] ";

        // Folosim metoda existentă din vRichEdit
        this->appendText(prefix + message + L"\n");
    }
};