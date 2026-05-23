#pragma once
#include "../ILayoutStrategy.hpp"
#include "../vContainer.hpp"
#include "../ConsoleManager.hpp"
#include "../../stringUtils.hpp"
#include <algorithm>
#include <vector>

class VerticalStackLayout : public ILayoutStrategy {
public:
    void applyLayout(vContainer& container) override {

        RECT rc;
        if (!GetClientRect(container.getHandle(), &rc)) return;

        int containerWidth = rc.right - rc.left;
        int containerHeight = rc.bottom - rc.top;
        if (containerWidth <= 0 || containerHeight <= 0) return;

        int currentDpi = container.getCurrentDpi();
        if (currentDpi == 0) currentDpi = 96;

        auto& children = container.getChildren();

        // --- PASUL 1: Calculăm înălțimea ocupată de elementele fixe (scalate) ---
        int occupiedHeight = 0;
        vControl* fillChild = nullptr;

        for (auto& entry : children) {

            auto& child = entry.second;

            //LOG_DEBUG(L"[applyLayout] la inceput Pozitionez " + str_to_wstr(child->getId()) +
              //  L" la " + std::to_wstring(child->getX()) + L"," +
              //  std::to_wstring(child->getY()) + L" size " +
              //  std::to_wstring(child->getWidth()) + L"x" +
              //  std::to_wstring(child->getHeight()));


            
            int mTop = MulDiv(child->getMarginTop(), currentDpi, 96);
            int mBottom = MulDiv(child->getMarginBottom(), currentDpi, 96);

            if (child->getHeightMode() == SizeMode::FILL) {
                fillChild = child.get();
                occupiedHeight += mTop + mBottom;
            }
            else {
                int baseH = MulDiv(child->getBaseHeight(), currentDpi, 96);
                occupiedHeight += baseH + mTop + mBottom;
            }

            //LOG_DEBUG(L"[applyLayout] la final Pozitionez " + str_to_wstr(child->getId()) +
              //  L" la " + std::to_wstring(child->getX()) + L"," +
              //  std::to_wstring(child->getY()) + L" size " +
              //  std::to_wstring(child->getWidth()) + L"x" +
              //  std::to_wstring(child->getHeight()));
        }

        // --- PASUL 2: Aplicăm poziționarea verticală ---
        int currentY = 0;
        for (auto& entry : children) {
            auto& child = entry.second;

            int mLeft = MulDiv(child->getMarginLeft(), currentDpi, 96);
            int mRight = MulDiv(child->getMarginRight(), currentDpi, 96);
            int mTop = MulDiv(child->getMarginTop(), currentDpi, 96);
            int mBottom = MulDiv(child->getMarginBottom(), currentDpi, 96);
            int baseW = MulDiv(child->getBaseWidth(), currentDpi, 96);
            int baseH = MulDiv(child->getBaseHeight(), currentDpi, 96);

            int targetX, targetY, targetW, targetH;

            // Calcul Lățime
            if (child->getWidthMode() == SizeMode::FILL) {
                targetX = mLeft;
                targetW = containerWidth - mLeft - mRight;
            }
            else {
                targetX = mLeft;
                targetW = baseW;
            }

            // Calcul Înălțime
            if (child.get() == fillChild) {
                targetH = containerHeight - occupiedHeight;
            }
            else {
                targetH = baseH;
            }

            targetY = currentY + mTop;

            child->moveAndResize(targetX, targetY, (std::max)(0, targetW), (std::max)(0, targetH));

            // Avansăm pentru următorul element
            currentY += targetH + mTop + mBottom;
        }
    }
};