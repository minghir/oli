#include "xPanel.hpp"
#include <iostream>

xPanel::xPanel(const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher)
    : xContainer(id, x, y, width, height, dispatcher), m_isPressed(false) {}

void xPanel::create(GtkWidget* parent) {
    // 1. Creăm un box (panou) generic în GTK
    // Folosim un GtkBox ca widget de layout pentru acest panou
    m_widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(m_widget, m_id.c_str());

    // 2. Setăm dimensiunile cerute
    gtk_widget_set_size_request(m_widget, m_width, m_height);

    // 3. Adăugăm widget-ul în părintele GTK
    if (GTK_IS_CONTAINER(parent)) {
        gtk_container_add(GTK_CONTAINER(parent), m_widget);
    }

    // 4. Conectăm evenimentele de mouse
    // GTK necesită activarea explicită a evenimentelor pentru widget
    gtk_widget_add_events(m_widget, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

    g_signal_connect(m_widget, "button-press-event", G_CALLBACK(+[](GtkWidget* w, GdkEventButton* e, gpointer data) -> gboolean {
        static_cast<xPanel*>(data)->onMouseClick((int)e->x, (int)e->y);
        return TRUE;
    }), this);

    gtk_widget_show_all(m_widget);
}

void xPanel::moveAndResize(int x, int y, int width, int height) {
    m_x = x; m_y = y;
    m_width = width; m_height = height;

    // În GTK, layout-ul este adesea gestionat de containerele părinte.
    // Totuși, putem forța o cerere de redimensionare:
    gtk_widget_set_size_request(m_widget, width, height);
}

void xPanel::onMouseClick(int x, int y) {
    m_isPressed = true;
    std::cout << "[xPanel] Click la: " << x << ", " << y << std::endl;
    onClick(); // Apelează metoda din xControl
}

void xPanel::onClick() {
    xControl::onClick();
}

bool xPanel::setProperty(const std::wstring& name, const vData& value) {
    if (name == L"layout") {
        std::wstring layoutStyle = value.toWString();
        
        if (m_widget && GTK_IS_ORIENTABLE(m_widget)) {
            if (layoutStyle == L"VSTACK") {
                gtk_orientable_set_orientation(GTK_ORIENTABLE(m_widget), GTK_ORIENTATION_VERTICAL);
                std::cout << "[xPanel] Layout schimbat la VSTACK pentru: " << m_id << std::endl;
                return true;
            } 
            else if (layoutStyle == L"HSTACK") {
                gtk_orientable_set_orientation(GTK_ORIENTABLE(m_widget), GTK_ORIENTATION_HORIZONTAL);
                std::cout << "[xPanel] Layout schimbat la HSTACK pentru: " << m_id << std::endl;
                return true;
            }
        }
    }

    // Dacă nu e proprietatea de layout, trimitem către containerul de bază
    return xContainer::setProperty(name, value);
}

vData xPanel::getProperty(const std::wstring& name) const {
    return xContainer::getProperty(name);
}

bool xPanel::callMethod(const std::wstring& methodName, const std::vector<vData>& args) {
    if (methodName == L"set_layout" && !args.empty()) {
        return this->setProperty(L"layout", args[0]);
    }
    
    return xContainer::callMethod(methodName, args);
}