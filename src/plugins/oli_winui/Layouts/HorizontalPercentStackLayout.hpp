#pragma once
#include "../ILayoutStrategy.hpp"
#include "../vContainer.hpp"
#include <algorithm>

class HorizontalPercentStackLayout : public ILayoutStrategy {
public:
    void applyLayout(vContainer& container) override {
        int containerWidth = container.getWidth();
        int containerHeight = container.getHeight();

        // Evităm calculele dacă containerul nu are încă dimensiuni
        if (containerWidth <= 0 || containerHeight <= 0) return;

        int currentDpi = container.getCurrentDpi();
        if (currentDpi == 0) currentDpi = 96;

        auto& children = container.getChildren();
        if (children.empty()) return;

        //LOG_DEBUG(L"[Layout] --- Start " + str_to_wstr(container.getId()) + L" | W_TOTAL: " + std::to_wstring(containerWidth) + L" ---");

        // --- PASUL 1: Analiză Resurse (Calculăm spațiul ocupat de elemente non-FILL) ---
        int occupiedWidth = 0;
        int fillCount = 0;

        for (auto const& [name, childPtr] : children) {
            int mL = MulDiv(childPtr->getMarginLeft(), currentDpi, 96);
            int mR = MulDiv(childPtr->getMarginRight(), currentDpi, 96);

            // Verificăm ce mod de dimensionare vede Layout-ul
            SizeMode mode = childPtr->getWidthMode();

            if (mode == SizeMode::PERCENT) {
                float ratio = (float)childPtr->getBaseWidth() / 100.0f;
                int px = (int)(containerWidth * ratio);
                occupiedWidth += px;
                //LOG_DEBUG(L"  [Check] PERCENT: " + str_to_wstr(childPtr->getId()) + L" (" + std::to_wstring(childPtr->getBaseWidth()) + L"%) = " + std::to_wstring(px) + L"px");
            }
            else if (mode == SizeMode::FIXED) {
                int px = MulDiv(childPtr->getBaseWidth(), currentDpi, 96) + mL + mR;
                occupiedWidth += px;
                //LOG_DEBUG(L"  [Check] FIXED: " + str_to_wstr(childPtr->getId()) + L" = " + std::to_wstring(px) + L"px (inc. margini)");
            }
            else if (mode == SizeMode::FILL) {
                fillCount++;
                occupiedWidth += (mL + mR); // Doar marginile ocupă spațiu garantat acum
            }
        }

        int remainingSpace = containerWidth - occupiedWidth;
        // Folosim std::max<int> conform sugestiei tale
        int fillWidthSingle = (fillCount > 0) ? (std::max<int>(0, remainingSpace / fillCount)) : 0;

        //LOG_DEBUG(L"  [Check] Ramas pt FILL: " + std::to_wstring(remainingSpace) + L" | Fiecare FILL primeste: " + std::to_wstring(fillWidthSingle));

        // --- PASUL 2: Poziționare Efectivă ---
        int currentX = 0;
        for (auto const& [name, childPtr] : children) {
            int mL = MulDiv(childPtr->getMarginLeft(), currentDpi, 96);
            int mR = MulDiv(childPtr->getMarginRight(), currentDpi, 96);
            int mT = MulDiv(childPtr->getMarginTop(), currentDpi, 96);
            int mB = MulDiv(childPtr->getMarginBottom(), currentDpi, 96);

            int targetW = 0;
            int nextStepX = 0; // Cât avansăm cursorul X pentru acest element

            if (childPtr->getWidthMode() == SizeMode::PERCENT) {
                nextStepX = (int)(containerWidth * ((float)childPtr->getBaseWidth() / 100.0f));
                targetW = nextStepX - (mL + mR);
            }
            else if (childPtr->getWidthMode() == SizeMode::FILL) {
                targetW = fillWidthSingle;
                nextStepX = targetW + mL + mR;
            }
            else { // FIXED
                targetW = MulDiv(childPtr->getBaseWidth(), currentDpi, 96);
                nextStepX = targetW + mL + mR;
            }

            targetW = (std::max<int>)(0, targetW);
            int targetH = (childPtr->getHeightMode() == SizeMode::FILL)
                ? (containerHeight - mT - mB)
                : MulDiv(childPtr->getBaseHeight(), currentDpi, 96);

            //LOG_DEBUG(L"  [Final] " + str_to_wstr(childPtr->getId()) + L" -> X: " + std::to_wstring(currentX + mL) + L" W: " + std::to_wstring(targetW));

            childPtr->moveAndResize(currentX + mL, mT, targetW, (std::max<int>)(0, targetH));

            // Avansăm cursorul pentru următorul control
            currentX += nextStepX;
        }
        //LOG_DEBUG(L"[Layout] --- End " + str_to_wstr(container.getId()) + L" ---\n");
    }
};