//test vscode
#include "xContainer.hpp"
#include "IXLayoutStrategy.hpp"
#include <algorithm>

xContainer::xContainer(const std::string& id, EventDispatcher& dispatcher)
    : xControl(id, dispatcher) {}

xContainer::xContainer(const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher)
    : xControl(id, x, y, width, height, dispatcher) {}

void xContainer::create(GtkWidget* parent) {
    m_widget = gtk_fixed_new(); 
    m_layoutWidget = m_widget;

    if (parent) {
        gtk_container_add(GTK_CONTAINER(parent), m_widget);
    }
}

void xContainer::applyLayout() {
    if (!m_widget) return;

    for (auto& entry : m_children) {
        xControl* child = entry.second.get();
        if (!child || !child->getHandle()) continue;

        GtkWidget* childWidget = child->getHandle();

        // 1. Mapare MARGINS (Oli margin -> GTK margins)
        int margin = (int)child->getProperty(L"margin").toInt();
        if (margin > 0) {
            gtk_widget_set_margin_start(childWidget, margin);
            gtk_widget_set_margin_end(childWidget, margin);
            gtk_widget_set_margin_top(childWidget, margin);
            gtk_widget_set_margin_bottom(childWidget, margin);
        }

        // 2. Mapare WIDTH_MODE ("FILL")
        std::wstring widthMode = child->getProperty(L"width_mode").toWString();
        if (widthMode == L"FILL") {
            gtk_widget_set_hexpand(childWidget, TRUE);
            gtk_widget_set_halign(childWidget, GTK_ALIGN_FILL);
        } else {
            gtk_widget_set_hexpand(childWidget, FALSE);
        }

        // 3. Mapare HEIGHT_MODE ("FILL")
        std::wstring heightMode = child->getProperty(L"height_mode").toWString();
        if (heightMode == L"FILL") {
            gtk_widget_set_vexpand(childWidget, TRUE);
            gtk_widget_set_valign(childWidget, GTK_ALIGN_FILL);
        } else {
            gtk_widget_set_vexpand(childWidget, FALSE);
        }

        // Propagare recursivă în sub-containere nativ
        xContainer* childCont = dynamic_cast<xContainer*>(child);
        if (childCont) {
            childCont->applyLayout();
        }
    }
}

bool xContainer::setProperty(const std::wstring& name, const vData& value) {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    if (prop == L"layout") {
        std::wstring layoutType = value.toWString();
        std::transform(layoutType.begin(), layoutType.end(), layoutType.begin(), ::toupper);

        if (m_layoutWidget && m_layoutWidget != m_widget) {
            gtk_widget_destroy(m_layoutWidget);
        }

        if (layoutType == L"VSTACK") {
            m_layoutWidget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
            gtk_container_add(GTK_CONTAINER(m_widget), m_layoutWidget);
            gtk_widget_show(m_layoutWidget);
        }
        else if (layoutType == L"HSTACK") {
            m_layoutWidget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
            gtk_container_add(GTK_CONTAINER(m_widget), m_layoutWidget);
            gtk_widget_show(m_layoutWidget);
        }
        else if (layoutType == L"GRID") {
            m_layoutWidget = gtk_grid_new();
            gtk_container_add(GTK_CONTAINER(m_widget), m_layoutWidget);
            gtk_widget_show(m_layoutWidget);
        }
        else {
            m_layoutWidget = m_widget; 
        }

        this->applyLayout();
        return true;
    }

    return xControl::setProperty(name, value);
}

vData xContainer::getProperty(const std::wstring& name) const {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    if (prop == L"layout") {
        if (GTK_IS_BOX(m_layoutWidget)) {
            GtkOrientation orient = gtk_orientable_get_orientation(GTK_ORIENTABLE(m_layoutWidget));
            return vData(orient == GTK_ORIENTATION_VERTICAL ? L"VSTACK" : L"HSTACK");
        }
        if (GTK_IS_GRID(m_layoutWidget)) return vData(L"GRID");
        return vData(L"FIXED");
    }

    return xControl::getProperty(name);
}