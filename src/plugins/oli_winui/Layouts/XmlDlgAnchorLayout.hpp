#pragma once
#include "../ILayoutStrategy.hpp"
#include "../vContainer.hpp"
#include <algorithm>

class XmlDlgAnchorLayout : public ILayoutStrategy {
public:
    void applyLayout(vContainer& container) override {
        HWND hContainer = container.getHandle();
        if (!hContainer) return;

        RECT rc;
        GetClientRect(hContainer, &rc);
        int currentWidth = rc.right - rc.left;
        int currentHeight = rc.bottom - rc.top;

        if (currentWidth <= 0 || currentHeight <= 0) return;

        int currentDpi = container.getCurrentDpi();
        if (currentDpi == 0) currentDpi = 96;

        const RECT& orig = container.getOriginalClientRect();
        int origWidth = orig.right - orig.left;
        int origHeight = orig.bottom - orig.top;

        // Delta în pixeli fizici (pentru modul scalare relativă)
        int deltaW = currentWidth - MulDiv(origWidth, currentDpi, 96);
        int deltaH = currentHeight - MulDiv(origHeight, currentDpi, 96);

        int statusBarH = 0;
        for (auto& entry : container.getChildren()) {
            if (entry.second->getType() == ControlType::StatusBar) {
                entry.second->resize();
                RECT r; GetWindowRect(entry.second->getHandle(), &r);
                statusBarH = r.bottom - r.top;
            }
        }

        int availableHeight = currentHeight - statusBarH;

        for (auto& entry : container.getChildren()) {
            auto& child = entry.second;
            if (child->getType() == ControlType::StatusBar) continue;

            // 1. Scalăm valorile de design (Logice -> Fizice la DPI curent)
            int bx = MulDiv(child->getBaseX(), currentDpi, 96);
            int by = MulDiv(child->getBaseY(), currentDpi, 96);
            int bw = MulDiv(child->getBaseWidth(), currentDpi, 96);
            int bh = MulDiv(child->getBaseHeight(), currentDpi, 96);

            int mLeft = MulDiv(child->getMarginLeft(), currentDpi, 96);
            int mRight = MulDiv(child->getMarginRight(), currentDpi, 96);
            int mTop = MulDiv(child->getMarginTop(), currentDpi, 96);
            int mBottom = MulDiv(child->getMarginBottom(), currentDpi, 96);

            int x = bx, y = by, w = bw, h = bh;

            Anchor anch = child->getAnchor();
            bool hasLeft = (int(anch) & int(Anchor::LEFT)) != 0;
            bool hasRight = (int(anch) & int(Anchor::RIGHT)) != 0;
            bool hasTop = (int(anch) & int(Anchor::TOP)) != 0;
            bool hasBottom = (int(anch) & int(Anchor::BOTTOM)) != 0;
            bool centerH = (int(anch) & int(Anchor::CENTER_H)) != 0;
            bool centerV = (int(anch) & int(Anchor::CENTER_V)) != 0;

            // 2. Logica Orizontală
            if (child->getWidthMode() == SizeMode::FILL) {
                x = (child->getBaseX() > 0) ? bx : mLeft;
                w = currentWidth - x - mRight;
            }
            else {
                if (centerH) {
                    x = (currentWidth - w) / 2;
                }
                else if (hasLeft && hasRight) {
                    w = bw + deltaW;
                }
                else if (hasRight) {
                    // Calculăm distanța față de dreapta în design, scalată
                    int distFromRight = MulDiv(origWidth - (child->getBaseX() + child->getBaseWidth()), currentDpi, 96);
                    x = currentWidth - distFromRight - w;
                }
            }

            // 3. Logica Verticală
            if (child->getHeightMode() == SizeMode::FILL) {
                y = (child->getBaseY() > 0) ? by : mTop;
                h = availableHeight - y - mBottom;
            }
            else {
                if (centerV) {
                    y = (availableHeight - h) / 2;
                }
                else if (hasTop && hasBottom) {
                    h = bh + deltaH;
                }
                else if (hasBottom) {
                    // Corecție critică: distanța față de fund trebuie calculată din valorile originale scalate
                    int designBottom = child->getBaseY() + child->getBaseHeight();
                    int distFromBottom = MulDiv(origHeight - designBottom, currentDpi, 96);
                    y = availableHeight - distFromBottom - h;
                }
            }

            child->moveAndResize(x, y, (std::max)(0, w), (std::max)(0, h));
        }
    }
};