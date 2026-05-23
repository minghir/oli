#pragma once
#include "../ILayoutStrategy.hpp"
#include <vector>
#include <algorithm>

class GridLayout : public ILayoutStrategy {
private:
    int m_rows;
    int m_cols;
    int m_gap; // Aceasta este valoarea de BAZĂ a gap-ului

public:
    GridLayout(int rows, int cols, int gap = 5)
        : m_rows(rows), m_cols(cols), m_gap(gap) {}

    void applyLayout(vContainer& container) override {
        RECT rc;
        if (!GetClientRect(container.getHandle(), &rc)) return;

        int width = rc.right - rc.left;
        int height = rc.bottom - rc.top;

        if (width <= 0 || height <= 0) return;

        // 1. Scalăm gap-ul conform DPI-ului curent al containerului
        int currentDpi = container.getCurrentDpi();
        if (currentDpi == 0) currentDpi = 96;

        int scaledGap = MulDiv(m_gap, currentDpi, 96);

        // 2. Calculăm spațiul util total (scăzând toate gap-urile dintre coloane/rânduri)
        // Avem gap și la margini, deci (cols + 1) gap-uri orizontale
        int totalGapWidth = (m_cols + 1) * scaledGap;
        int totalGapHeight = (m_rows + 1) * scaledGap;

        // 3. Calculăm dimensiunea unei singure celule (folosim float pentru precizie la împărțire)
        float cellWidth = (float)(width - totalGapWidth) / (float)m_cols;
        float cellHeight = (float)(height - totalGapHeight) / (float)m_rows;

        // Protecție împotriva dimensiunilor negative
        cellWidth = (std::max)(0.0f, cellWidth);
        cellHeight = (std::max)(0.0f, cellHeight);

        for (auto& entry : container.getChildren()) {
            auto& child = entry.second;

            int r = child->getGridRow();
            int c = child->getGridColumn();

            // Ne asigurăm că nu ieșim din matricea definită (clamping)
            r = (std::max)(0, (std::min)(r, m_rows - 1));
            c = (std::max)(0, (std::min)(c, m_cols - 1));

            // 4. Calculăm poziția X și Y
            // Formula: Marginea inițială (gap) + coloana * (lățime celulă + gap-ul dintre ele)
            int x = scaledGap + c * (int)(cellWidth + scaledGap);
            int y = scaledGap + r * (int)(cellHeight + scaledGap);

            // 5. Aplicăm dimensiunile
            // Notă: (int)cellWidth face floor, ceea ce e ok pentru a evita depășirea pixelilor
            child->moveAndResize(x, y, (int)cellWidth, (int)cellHeight);
        }
    }
};