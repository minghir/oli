#pragma once
#include "../ILayoutStrategy.hpp"
#include "../vContainer.hpp"
#include <algorithm>

class FlowLayout : public ILayoutStrategy {
public:
    void applyLayout(vContainer& container) override {
        HWND hContainer = container.getHandle();
        if (!hContainer) return;

        RECT rc;
        GetClientRect(hContainer, &rc);
        int containerWidth = rc.right - rc.left;

        int currentDpi = container.getCurrentDpi();
        if (currentDpi == 0) currentDpi = 96;

        // Scalăm setările de bază ale layout-ului
        const int MARGIN_X = MulDiv(10, currentDpi, 96);
        const int MARGIN_Y = MulDiv(10, currentDpi, 96);
        const int GAP = MulDiv(5, currentDpi, 96);

        int currentX = MARGIN_X;
        int currentY = MARGIN_Y;
        int maxRowHeight = 0;

        for (auto& entry : container.getChildren()) {
            auto& child = entry.second;

            // 1. Scalăm dimensiunile de bază ale controlului
            int childWidth = MulDiv(child->getBaseWidth(), currentDpi, 96);
            int childHeight = MulDiv(child->getBaseHeight(), currentDpi, 96);

            // 2. Luăm în calcul marginile specifice ale controlului (scalate)
            int mLeft = MulDiv(child->getMarginLeft(), currentDpi, 96);
            int mRight = MulDiv(child->getMarginRight(), currentDpi, 96);
            int mTop = MulDiv(child->getMarginTop(), currentDpi, 96);
            int mBottom = MulDiv(child->getMarginBottom(), currentDpi, 96);

            // Spațiul total ocupat orizontal de acest control
            int totalChildWidth = mLeft + childWidth + mRight;

            // 3. Logica de Wrap (Trecere la linie nouă)
            // Verificăm dacă elementul (cu tot cu marginile lui) depășește lățimea utilă
            if (currentX + totalChildWidth + MARGIN_X > containerWidth) {
                if (currentX != MARGIN_X) {
                    currentX = MARGIN_X;
                    currentY += maxRowHeight + GAP;
                    maxRowHeight = 0;
                }
            }

            // 4. Poziționare (X și Y includ marginile controlului)
            child->moveAndResize(
                currentX + mLeft,
                currentY + mTop,
                childWidth,
                childHeight
            );

            // 5. Avansare cursor
            currentX += totalChildWidth + GAP;

            // Calculăm înălțimea rândului incluzând marginile de sus/jos ale controlului
            maxRowHeight = (std::max)(maxRowHeight, mTop + childHeight + mBottom);
        }
    }
};