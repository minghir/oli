#ifndef XVERTICALPERCENTSTACKLAYOUT_HPP
#define XVERTICALPERCENTSTACKLAYOUT_HPP

#pragma once
#include "../IXLayoutStrategy.hpp"
#include "../xContainer.hpp"
#include "../xControl.hpp"
#include <algorithm>
#include <gtk/gtk.h>

// Emulator safe pentru funcția MulDiv din Windows
inline int LinuxMulDiv(int number, int numerator, int denominator) {
    if (denominator == 0) return 0;
    return static_cast<int>((static_cast<long long>(number) * numerator) / denominator);
}

class XVerticalPercentStackLayout : public IXLayoutStrategy {
public:
    void applyLayout(xContainer& container) override {
        GtkWidget* widget = container.getHandle();
        if (!widget) return;

        // În GTK, obținem dimensiunile curente ale containerului folosind GtkAllocation
        GtkAllocation allocation;
        gtk_widget_get_allocation(widget, &allocation);

        int containerWidth = allocation.width;
        int containerHeight = allocation.height;

        // Prevenim rularea pe dimensiuni invalide la inițializare
        if (containerWidth <= 10 || containerHeight <= 10) return;

        int currentDpi = 96; // Se poate extinde ulterior dacă ai un DPI manager pe Linux
        auto& children = container.getChildren();

        // --- PASUL 1: Calcul spațiu fix (Doar pentru elemente vizibile) ---
        int heightUsedByFixed = 0;

        for (auto& entry : children) {
            auto& child = entry.second;
            if (!child->isVisible()) continue;

            int mTop = LinuxMulDiv(child->getMarginTop(), currentDpi, 96);
            int mBottom = LinuxMulDiv(child->getMarginBottom(), currentDpi, 96);

            if (child->getHeightMode() == SizeMode::FIXED) {
                int baseH = LinuxMulDiv(child->getBaseHeight(), currentDpi, 96);
                heightUsedByFixed += baseH + mTop + mBottom;
            } else {
                heightUsedByFixed += mTop + mBottom;
            }
        }

        int remainingHeight = (std::max)(0, containerHeight - heightUsedByFixed);

        // --- PASUL 2: Poziționarea absolută în GtkFixed ---
        int currentY = 0;

        for (auto& entry : children) {
            auto& child = entry.second;
            if (!child->isVisible()) continue;

            int mLeft = LinuxMulDiv(child->getMarginLeft(), currentDpi, 96);
            int mRight = LinuxMulDiv(child->getMarginRight(), currentDpi, 96);
            int mTop = LinuxMulDiv(child->getMarginTop(), currentDpi, 96);
            int mBottom = LinuxMulDiv(child->getMarginBottom(), currentDpi, 96);

            int targetW, targetH;

            // Calcul Lățime (FILL sau FIXED)
            if (child->getWidthMode() == SizeMode::FILL) {
                targetW = containerWidth - mLeft - mRight;
            } else {
                targetW = LinuxMulDiv(child->getBaseWidth(), currentDpi, 96);
            }

            // Calcul Înălțime (PERCENT, FILL sau FIXED)
            if (child->getHeightMode() == SizeMode::PERCENT) {
                float percent = static_cast<float>(child->getBaseHeight()) / 100.0f;
                targetH = static_cast<int>(containerHeight * percent);
            } else if (child->getHeightMode() == SizeMode::FILL) {
                targetH = remainingHeight;
            } else {
                targetH = LinuxMulDiv(child->getBaseHeight(), currentDpi, 96);
            }

            int targetX = mLeft;
            int targetY = currentY + mTop;

            // Mutăm fizic widget-ul în GtkFixed
            child->moveAndResize(targetX, targetY, (std::max)(0, targetW), (std::max)(0, targetH));

            currentY += targetH + mTop + mBottom;
        }
    }
};

#endif // XVERTICALPERCENTSTACKLAYOUT_HPP