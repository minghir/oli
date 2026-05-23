#pragma once
#include "../ILayoutStrategy.hpp"
#include "../vContainer.hpp"
#include <algorithm>

class VerticalPercentStackLayout : public ILayoutStrategy {
public:
    /*
    void applyLayout(vContainer& container) override {
        RECT rc;
        if (!GetClientRect(container.getHandle(), &rc)) return;

        int containerWidth = rc.right - rc.left;
        int containerHeight = rc.bottom - rc.top;
        if (containerWidth <= 0 || containerHeight <= 0) return;

        int currentDpi = container.getCurrentDpi();
        if (currentDpi == 0) currentDpi = 96;

        auto& children = container.getChildren();

        // --- PASUL 1: Identificăm tipurile de copii și calculăm spațiul fix ---
        int heightUsedByFixed = 0;

        for (auto& entry : children) {
            auto& child = entry.second;
            int mTop = MulDiv(child->getMarginTop(), currentDpi, 96);
            int mBottom = MulDiv(child->getMarginBottom(), currentDpi, 96);

            // Dacă înălțimea e mică (ex: < 100), o tratăm ca procent? 
            // Sau mai bine adaugi o metodă child->getHeightIsPercent()
            if (child->getHeightMode() == SizeMode::FIXED) {
                int baseH = MulDiv(child->getBaseHeight(), currentDpi, 96);
                heightUsedByFixed += baseH + mTop + mBottom;
            }
            else {
                // FILL sau PERCENT doar adună marginile la spațiul ocupat "fix"
                heightUsedByFixed += mTop + mBottom;
            }
        }

        int remainingHeight = (std::max)(0, containerHeight - heightUsedByFixed);

        // --- PASUL 2: Poziționarea ---
        int currentY = 0;

        for (auto& entry : children) {
            auto& child = entry.second;

            int mLeft = MulDiv(child->getMarginLeft(), currentDpi, 96);
            int mRight = MulDiv(child->getMarginRight(), currentDpi, 96);
            int mTop = MulDiv(child->getMarginTop(), currentDpi, 96);
            int mBottom = MulDiv(child->getMarginBottom(), currentDpi, 96);

            int targetW, targetH;

            // Lățime (rămâne neschimbată logica)
            if (child->getWidthMode() == SizeMode::FILL) {
                targetW = containerWidth - mLeft - mRight;
            }
            else {
                targetW = MulDiv(child->getBaseWidth(), currentDpi, 96);
            }

            // ÎNĂLȚIME (Logica de Procent)
            if (child->getHeightMode() == SizeMode::PERCENT) {
                // Dacă baseHeight este 50, calculăm 50% din înălțimea TOTALĂ a containerului
                // sau din cea rămasă? De obicei, procentul se referă la părinte.
                float percent = (float)child->getBaseHeight() / 100.0f;
                targetH = (int)(containerHeight * percent);
            }
            else if (child->getHeightMode() == SizeMode::FILL) {
                // FILL ocupă ce a mai rămas (dacă ai mai multe, ar trebui împărțit)
                targetH = remainingHeight;
            }
            else {
                // FIXED
                targetH = MulDiv(child->getBaseHeight(), currentDpi, 96);
            }

            int targetX = mLeft;
            int targetY = currentY + mTop;

            child->moveAndResize(targetX, targetY, (std::max)(0, targetW), (std::max)(0, targetH));

            currentY += targetH + mTop + mBottom;
        }
    }
    */
    void applyLayout(vContainer& container) {
        RECT rc;
        if (!GetClientRect(container.getHandle(), &rc)) return;

        int containerWidth = rc.right - rc.left;
        int containerHeight = rc.bottom - rc.top;
        if (containerWidth <= 0 || containerHeight <= 0) return;

        int currentDpi = container.getCurrentDpi();
        if (currentDpi == 0) currentDpi = 96;

        auto& children = container.getChildren();

        // --- PASUL 1: Calcul spațiu fix (Doar pentru elemente vizibile) ---
        int heightUsedByFixed = 0;

        for (auto& entry : children) {
            auto& child = entry.second;

            // DACĂ E ASCUNS, NU OCUPĂ SPAȚIU
            if (!child->isVisible()) continue;

            int mTop = MulDiv(child->getMarginTop(), currentDpi, 96);
            int mBottom = MulDiv(child->getMarginBottom(), currentDpi, 96);

            if (child->getHeightMode() == SizeMode::FIXED) {
                int baseH = MulDiv(child->getBaseHeight(), currentDpi, 96);
                heightUsedByFixed += baseH + mTop + mBottom;
            }
            else {
                heightUsedByFixed += mTop + mBottom;
            }
        }

        int remainingHeight = (std::max)(0, containerHeight - heightUsedByFixed);

        // --- PASUL 2: Poziționarea ---
        int currentY = 0;

        for (auto& entry : children) {
            auto& child = entry.second;

            // DACĂ E ASCUNS, ÎL SĂRIM (Rămâne la coordonatele vechi sau invizibil)
            if (!child->isLogicVisible()) continue;

            int mLeft = MulDiv(child->getMarginLeft(), currentDpi, 96);
            int mRight = MulDiv(child->getMarginRight(), currentDpi, 96);
            int mTop = MulDiv(child->getMarginTop(), currentDpi, 96);
            int mBottom = MulDiv(child->getMarginBottom(), currentDpi, 96);

            int targetW, targetH;

            // Lățime
            if (child->getWidthMode() == SizeMode::FILL) {
                targetW = containerWidth - mLeft - mRight;
            }
            else {
                targetW = MulDiv(child->getBaseWidth(), currentDpi, 96);
            }

            // Înălțime
            if (child->getHeightMode() == SizeMode::PERCENT) {
                float percent = (float)child->getBaseHeight() / 100.0f;
                targetH = (int)(containerHeight * percent);
            }
            else if (child->getHeightMode() == SizeMode::FILL) {
                targetH = remainingHeight;
            }
            else {
                targetH = MulDiv(child->getBaseHeight(), currentDpi, 96);
            }

            int targetX = mLeft;
            int targetY = currentY + mTop;

            child->moveAndResize(targetX, targetY, (std::max)(0, targetW), (std::max)(0, targetH));

            // currentY crește doar pentru elementul desenat
            currentY += targetH + mTop + mBottom;
        }
    }
};