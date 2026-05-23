#pragma once
#include "../ILayoutStrategy.hpp"
#include "../vContainer.hpp"
#include <algorithm>
/*
class AnchorLayout : public ILayoutStrategy {
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

        // Delta în pixeli fizici
        int deltaW = currentWidth - origWidth;
        int deltaH = currentHeight - origHeight;

        int statusBarH = 0;
        // Identificăm dacă avem un status bar printre copii
        for (auto& entry : container.getChildren()) {
            if (entry.second->getType() == ControlType::StatusBar) {
                entry.second->resize(); // Trimite WM_SIZE, el se duce jos singur
                RECT r; GetWindowRect(entry.second->getHandle(), &r);
                statusBarH = r.bottom - r.top;
            }
        }

        int availableHeight = currentHeight - statusBarH;

        for (auto& entry : container.getChildren()) {

            auto& child = entry.second;

            if (child->getType() == ControlType::StatusBar) continue;

            // 1. Scalăm valorile de bază (Logice -> Fizice)
            int x = MulDiv(child->getBaseX(), currentDpi, 96);
            int y = MulDiv(child->getBaseY(), currentDpi, 96);
            int w = MulDiv(child->getBaseWidth(), currentDpi, 96);
            int h = MulDiv(child->getBaseHeight(), currentDpi, 96);

            int mLeft = MulDiv(child->getMarginLeft(), currentDpi, 96);
            int mRight = MulDiv(child->getMarginRight(), currentDpi, 96);
            int mTop = MulDiv(child->getMarginTop(), currentDpi, 96);
            int mBottom = MulDiv(child->getMarginBottom(), currentDpi, 96);

            Anchor anch = child->getAnchor();
            bool hasLeft = (int(anch) & int(Anchor::LEFT)) != 0;
            bool hasRight = (int(anch) & int(Anchor::RIGHT)) != 0;
            bool hasTop = (int(anch) & int(Anchor::TOP)) != 0;
            bool hasBottom = (int(anch) & int(Anchor::BOTTOM)) != 0;

            // 2. Logica Orizontală
            if (child->getWidthMode() == SizeMode::FILL) {
                x = mLeft;
                w = currentWidth - mLeft - mRight;
            }
            else {
                if (hasLeft && hasRight) w += deltaW;
                else if (hasRight) x += deltaW;
            }

            // 3. Logica Verticală
            if (child->getHeightMode() == SizeMode::FILL) {
                y = mTop;
                //h = currentHeight - mTop - mBottom;
                h = availableHeight - mTop - mBottom;
            }
            else {
                if (hasTop && hasBottom) h += deltaH;
                //else if (hasBottom) y += deltaH;
                else if (hasBottom) y += (currentHeight - origHeight);
            }

            // 4. Aplicăm transformarea
            child->moveAndResize(x, y, (std::max)(0, w), (std::max)(0, h));
        }
    }
};
*/


class AnchorLayout : public ILayoutStrategy {
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

        int deltaW = (origWidth > 0) ? (currentWidth - origWidth) : 0;
        int deltaH = (origHeight > 0) ? (currentHeight - origHeight) : 0;

        // Detectare Status Bar
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

            // 1. Scalăm coordonatele și dimensiunile de bază
            int x = MulDiv(child->getBaseX(), currentDpi, 96);
            int y = MulDiv(child->getBaseY(), currentDpi, 96);
            int w = MulDiv(child->getBaseWidth(), currentDpi, 96);
            int h = MulDiv(child->getBaseHeight(), currentDpi, 96);

            int mLeft = MulDiv(child->getMarginLeft(), currentDpi, 96);
            int mRight = MulDiv(child->getMarginRight(), currentDpi, 96);
            int mTop = MulDiv(child->getMarginTop(), currentDpi, 96);
            int mBottom = MulDiv(child->getMarginBottom(), currentDpi, 96);

            Anchor anch = child->getAnchor();

            // 2. LOGICA ORIZONTALĂ
            if (hasFlag(anch, Anchor::CENTER_H)) {
                // Centrare: (Lățime container - Lățime control) / 2
                x = (currentWidth - w) / 2;
            }
            else if (child->getWidthMode() == SizeMode::FILL) {
                x = mLeft;
                w = currentWidth - mLeft - mRight;
            }
            else {
                if (hasFlag(anch, Anchor::LEFT) && hasFlag(anch, Anchor::RIGHT)) {
                    w += deltaW;
                }
                else if (hasFlag(anch, Anchor::RIGHT)) {
                    x += deltaW;
                }
            }

            // 3. LOGICA VERTICALĂ
            if (hasFlag(anch, Anchor::CENTER_V)) {
                y = (availableHeight - h) / 2;
            }
            else if (child->getHeightMode() == SizeMode::FILL) {
                y = mTop;
                h = availableHeight - mTop - mBottom;
            }
            else if (hasFlag(anch, Anchor::TOP) && hasFlag(anch, Anchor::BOTTOM)) {
                // Se întinde pe verticală între margini
                y = mTop;
                h = availableHeight - mTop - mBottom;
            }
            else if (hasFlag(anch, Anchor::BOTTOM)) {
                // FIX: Poziționare fixă față de fundul ferestrei
                // y = Înălțime totală - înălțime control - marginea de jos
                y = availableHeight - h - mBottom;
            }
            else {
                // Implicit: Anchor::TOP
                y = mTop;
            }

            // 4. Constrângeri de siguranță (Min/Max size)
            w = (std::max)(child->getMinWidth(), w);
            h = (std::max)(child->getMinHeight(), h);

            // 5. Aplicăm transformarea
            child->moveAndResize(x, y, (std::max)(0, w), (std::max)(0, h));
        }
    }
};