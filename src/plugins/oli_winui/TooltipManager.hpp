// TooltipManager.hpp

#ifndef TOOLTIP_MANAGER_HPP
#define TOOLTIP_MANAGER_HPP

#include <windows.h>
#include <string>

class TooltipManager {
public:
    // Metoda statică pentru a obține instanța Singleton
    static TooltipManager& getInstance();

    // Nu permit copierea sau asignarea
    TooltipManager(const TooltipManager&) = delete;
    TooltipManager& operator=(const TooltipManager&) = delete;

    /**
     * @brief Adaugă un tooltip unui control.
     * @param hControl Handle-ul controlului la care se adaugă tooltip-ul.
     * @param tooltipText Textul de afișat în tooltip.
     */
    void addTooltip(HWND hControl, const std::wstring& tooltipText);

    /**
     * @brief Setează fontul pentru toate tooltip-urile.
     * @param hFont Handle-ul fontului.
     */
    void setFont(HFONT hFont);

    // Distruge controlul tooltip la închiderea aplicației.
    void cleanup();

private:
    // Constructor și destructor privați (specific Singleton)
    TooltipManager();
    ~TooltipManager();

    // Handle-ul controlului de tip tooltip
    HWND m_tooltipHandle = nullptr;
};

#endif // TOOLTIP_MANAGER_HPP