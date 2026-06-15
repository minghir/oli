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

    g_signal_connect(m_widget, "size-allocate", G_CALLBACK(+[](GtkWidget*, GdkRectangle*, gpointer data) {
    auto* self = static_cast<xContainer*>(data);
    if (self) {
        self->applyLayout(); // Se auto-apelează la orice resize de fereastră!
    }
}), this);
}

void xContainer::applyLayout() {
        if (m_layoutStrategy) {
            m_layoutStrategy->applyLayout(*this);
        }

        // Propagăm recursiv layout-ul către toți copiii care sunt la rândul lor containere
        for (auto& entry : m_children) {
            xContainer* childCont = dynamic_cast<xContainer*>(entry.second.get());
            if (childCont && childCont->isVisible()) {
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