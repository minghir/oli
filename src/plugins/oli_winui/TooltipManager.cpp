// TooltipManager.cpp

#define WINVER 0x0600
#define _WIN32_WINNT 0x0600
#define _WIN32_IE 0x0600 // This is also good practice to include

#include "TooltipManager.hpp"
#include "ConsoleManager.hpp" // Adjust the path

#include <windows.h>
#include <CommCtrl.h>
#pragma comment(lib, "Comctl32.lib")

TooltipManager& TooltipManager::getInstance() {
    static TooltipManager instance;
    return instance;
}

TooltipManager::TooltipManager() {
    // Se asigură că controalele comune sunt inițializate
    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
//    iccex.dwICC = ICC_TOOLTIP_CLASSES;
    iccex.dwICC = ICC_TAB_CLASSES ;
    if (!InitCommonControlsEx(&iccex)) {
        ConsoleManager::getInstance().log(L"[ERROR] TooltipManager::TooltipManager: Nu s-au putut inițializa clasele de tooltip.");
        return;
    }

    // Creează controlul de tip tooltip, care va fi un singur handle pentru toate tooltip-urile
    m_tooltipHandle = CreateWindowEx(
        WS_EX_TOPMOST,
        TOOLTIPS_CLASS,
        NULL,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, // Nu are nevoie de un părinte inițial
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (!m_tooltipHandle) {
        ConsoleManager::getInstance().log(L"[ERROR] TooltipManager::TooltipManager: Eșec la crearea controlului de tip tooltip.");
    }
}

TooltipManager::~TooltipManager() {
    if (m_tooltipHandle) {
        DestroyWindow(m_tooltipHandle);
    }
}

void TooltipManager::addTooltip(HWND hControl, const std::wstring& tooltipText) {
    if (!m_tooltipHandle || !IsWindow(hControl)) {
        ConsoleManager::getInstance().log(L"[WARNING] TooltipManager::addTooltip: Handle invalid. Tooltip-ul nu a putut fi adăugat.");
        return;
    }

    TOOLINFOW ti = { 0 };
    ti.cbSize = sizeof(TOOLINFOW);
    ti.hwnd = hControl;
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND; // Use the HWND as the ID
    ti.lpszText = const_cast<LPWSTR>(tooltipText.c_str());

    // Add the tool to the tooltip control
    if (!SendMessage(m_tooltipHandle, TTM_ADDTOOL, 0, (LPARAM)&ti)) {
        ConsoleManager::getInstance().log(L"[ERROR] TooltipManager::addTooltip: Eșec la adăugarea tooltip-ului la control.");
    }
    else {
        ConsoleManager::getInstance().log(L"[INFO] TooltipManager::addTooltip: Tooltip adăugat cu succes pentru HWND.");
    }
}

void TooltipManager::setFont(HFONT hFont) {
    if (m_tooltipHandle) {
        SendMessage(m_tooltipHandle, WM_SETFONT, (WPARAM)hFont, FALSE);
    }
}

void TooltipManager::cleanup() {
    // Destructorul va face curățarea, dar această metodă poate fi apelată explicit
    if (m_tooltipHandle) {
        DestroyWindow(m_tooltipHandle);
        m_tooltipHandle = nullptr;
    }
}