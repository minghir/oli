#pragma once
#include "../ILayoutStrategy.hpp"
#include "../vContainer.hpp"
#include <vector>
#include <algorithm>

class FlexStackLayout : public ILayoutStrategy {
public:
    /*
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

        auto& children = container.getChildren();

        // --- PASUL 1: Calculăm spațiul orizontal ocupat (valori scalate) ---
        int totalFixedWidth = 0;
        int spacerCount = 0;

        for (auto& entry : children) {
            auto& child = entry.second;
            if (child->getWidthMode() != SizeMode::FILL) {
                if (child->isSpacer()) {
                    spacerCount++;
                }
                else {
                    int mLeft = MulDiv(child->getMarginLeft(), currentDpi, 96);
                    int mRight = MulDiv(child->getMarginRight(), currentDpi, 96);
                    int baseW = MulDiv(child->getBaseWidth(), currentDpi, 96);
                    totalFixedWidth += baseW + mLeft + mRight;
                }
            }
        }

        int remainingSpace = (std::max)(0, currentWidth - totalFixedWidth);

        // --- PASUL 2: Aplicăm Layout-ul ---
        int currentX = 0;
        int tempRemaining = remainingSpace;
        int activeSpacerCount = spacerCount;

        for (auto& entry : children) {
            auto& child = entry.second;

            int mLeft = MulDiv(child->getMarginLeft(), currentDpi, 96);
            int mRight = MulDiv(child->getMarginRight(), currentDpi, 96);
            int mTop = MulDiv(child->getMarginTop(), currentDpi, 96);
            int mBottom = MulDiv(child->getMarginBottom(), currentDpi, 96);
            int baseW = MulDiv(child->getBaseWidth(), currentDpi, 96);
            int baseH = MulDiv(child->getBaseHeight(), currentDpi, 96);

            int targetX, targetWidth, targetY, targetHeight;

            // LOGICĂ ORIZONTALĂ
            if (child->getWidthMode() == SizeMode::FILL) {
                targetX = mLeft;
                targetWidth = currentWidth - mLeft - mRight;
            }
            else {
                targetX = currentX + mLeft;
                if (child->isSpacer()) {
                    targetWidth = (activeSpacerCount > 0) ? (tempRemaining / activeSpacerCount) : 0;
                    tempRemaining -= targetWidth;
                    activeSpacerCount--;
                }
                else {
                    targetWidth = baseW;
                }
                currentX += targetWidth + mLeft + mRight;
            }

            // LOGICĂ VERTICALĂ (Centrare automată dacă nu e FILL)
            if (child->getHeightMode() == SizeMode::FILL) {
                targetY = mTop;
                targetHeight = currentHeight - mTop - mBottom;
            }
            else {
                // Dacă baseH este 0 sau nu e setat, ocupăm tot spațiul (comportament de siguranță)
                int h = (baseH > 0) ? baseH : currentHeight;

                // Constrângere: Nu lăsăm controlul să fie mai înalt decât containerul minus marginile
                int maxAvailableH = currentHeight - mTop - mBottom;
                if (h > maxAvailableH) h = maxAvailableH;

                // Centrare verticală matematică
                targetY = mTop + (maxAvailableH - h) / 2;
                targetHeight = h;

                // FIX pentru vEdit: 
                // Dacă diferența dintre înălțimea containerului și h este mică (ex: sub 5 pixeli),
                // forțăm FILL. Asta ajută controlul Edit să își gestioneze singur textul intern.
                if (maxAvailableH - h < 5) {
                    targetY = mTop;
                    targetHeight = maxAvailableH;
                }
            }
           

            child->moveAndResize(targetX, targetY, (std::max)(0, targetWidth), (std::max)(0, targetHeight));
        }
    }
    */
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

    auto& children = container.getChildren();

    // --- PASUL 1: Calculăm spațiul orizontal ocupat și numărăm elementele flexibile ---
    int totalUsedWidth = 0;
    int flexibleItemsCount = 0;

    for (auto& entry : children) {
        auto& child = entry.second;

        int mLeft = MulDiv(child->getMarginLeft(), currentDpi, 96);
        int mRight = MulDiv(child->getMarginRight(), currentDpi, 96);

        // Un element este "flexibil" dacă este Spacer SAU are WidthMode::FILL
        if (child->isSpacer() || child->getWidthMode() == SizeMode::FILL) {
            flexibleItemsCount++;
            // Chiar și elementele flexibile pot avea margini care ocupă spațiu fix
            totalUsedWidth += mLeft + mRight;
        }
        else {
            int baseW = MulDiv(child->getBaseWidth(), currentDpi, 96);
            totalUsedWidth += baseW + mLeft + mRight;
        }
    }

    // Spațiul rămas care va fi împărțit egal între Spacere și elementele FILL
    int remainingSpace = (std::max)(0, currentWidth - totalUsedWidth);

    // --- PASUL 2: Aplicăm poziționarea și redimensionarea ---
    int currentX = 0;
    int tempRemaining = remainingSpace;
    int activeFlexibleCount = flexibleItemsCount;

    for (auto& entry : children) {
        auto& child = entry.second;

        int mLeft = MulDiv(child->getMarginLeft(), currentDpi, 96);
        int mRight = MulDiv(child->getMarginRight(), currentDpi, 96);
        int mTop = MulDiv(child->getMarginTop(), currentDpi, 96);
        int mBottom = MulDiv(child->getMarginBottom(), currentDpi, 96);
        int baseW = MulDiv(child->getBaseWidth(), currentDpi, 96);
        int baseH = MulDiv(child->getBaseHeight(), currentDpi, 96);

        int targetX, targetWidth, targetY, targetHeight;

        // --- LOGICĂ ORIZONTALĂ (X și Width) ---
        targetX = currentX + mLeft;

        if (child->isSpacer() || child->getWidthMode() == SizeMode::FILL) {
            // Distribuim spațiul rămas. Folosim tempRemaining / activeFlexibleCount 
            // pentru a evita golurile de 1px cauzate de rotunjiri.
            int distributedWidth = (activeFlexibleCount > 0) ? (tempRemaining / activeFlexibleCount) : 0;

            targetWidth = distributedWidth;

            tempRemaining -= distributedWidth;
            activeFlexibleCount--;
        }
        else {
            targetWidth = baseW;
        }

        // Avansăm cursorul X pentru următorul element
        currentX += targetWidth + mLeft + mRight;

        // --- LOGICĂ VERTICALĂ (Y și Height) ---
        int maxAvailableH = currentHeight - mTop - mBottom;

        if (child->getHeightMode() == SizeMode::FILL) {
            targetY = mTop;
            targetHeight = (std::max)(0, maxAvailableH);
        }
        else {
            // Dacă baseH nu e setat (0), încercăm să ocupăm tot spațiul disponibil
            int h = (baseH > 0) ? baseH : currentHeight;
            if (h > maxAvailableH) h = maxAvailableH;

            // Centrare verticală matematică în spațiul util (minus margini)
            targetY = mTop + (maxAvailableH - h) / 2;
            targetHeight = h;

            // FIX pentru vEdit / Comenzi Win32: 
            // Dacă înălțimea calculată e foarte aproape de cea a containerului, 
            // forțăm FILL pentru a lăsa Windows-ul să gestioneze desenarea textului.
            if (maxAvailableH - h < 5) {
                targetY = mTop;
                targetHeight = (std::max)(0, maxAvailableH);
            }
        }

        // --- APLICARE ---
        child->moveAndResize(targetX, targetY, (std::max)(0, targetWidth), (std::max)(0, targetHeight));
    }
}
};