#pragma once

#include "../ILayoutStrategy.hpp"
#include "../vContainer.hpp"
#include "../ConsoleManager.hpp"
#include "../stringUtils.hpp"
#include <algorithm>
#include <vector>

class FormLayout : public ILayoutStrategy {
public:
    void applyLayout(vContainer& container) {
        HWND hContainer = container.getHandle();
        if (!hContainer) return;

        RECT rc;
        GetClientRect(hContainer, &rc);
        int currentWidth = rc.right - rc.left;
        int currentDpi = container.getCurrentDpi() ? container.getCurrentDpi() : 96;

        auto& children = container.getChildren(); // Vectorul de perechi

        int startX = MulDiv(20, currentDpi, 96);
        int currentY = MulDiv(20, currentDpi, 96);
        int labelWidth = MulDiv(120, currentDpi, 96);
        int rowHeight = MulDiv(25, currentDpi, 96);
        int verticalSpacing = MulDiv(10, currentDpi, 96);
        int horizontalGap = MulDiv(10, currentDpi, 96);

        for (size_t i = 0; i < children.size(); ++i) {
            auto& child = children[i].second;
            std::string id = children[i].first;

            // Verificăm dacă ID-ul începe cu "lbl_"
            if (id.find("lbl_") == 0) {
                // 1. Poziționăm Label-ul
                child->moveAndResize(startX, currentY + 3, labelWidth, rowHeight);

                // 2. Verificăm dacă următorul element este Edit-ul corespunzător
                if (i + 1 < children.size()) {
                    auto& nextChild = children[i + 1].second;
                    std::string nextId = children[i + 1].first;

                    if (nextId.find("edit_") == 0) {
                        int editX = startX + labelWidth + horizontalGap;
                        int editWidth = currentWidth - editX - startX;

                        // Constrângem lățimea să nu fie negativă
                        if (editWidth < 50) editWidth = 200;

                        nextChild->moveAndResize(editX, currentY, editWidth, rowHeight);

                        // Incrementăm 'i' pentru a sări peste Edit la următoarea iterație a for-ului
                        i++;
                    }
                }

                // Doar după ce am procesat un rând (Label + eventual Edit), coborâm pe Y
                currentY += rowHeight + verticalSpacing;
            }
        }
    }
};