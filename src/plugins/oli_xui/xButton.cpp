#include "../../StringUtils.hpp"
#include "xButton.hpp"
#include <algorithm>

xButton::xButton(const std::string& id, const std::wstring& text, int x, int y, int w, int h, EventDispatcher& dispatcher)
    : xControl(id, x, y, w, h, dispatcher), m_text(text) {
    m_ControlType = ControlType::Button;
}

// Callback static de tip C cerut de GObject Signal System
static void GtkButtonCallbackAdapter(GtkWidget* /*widget*/, gpointer data) {
    xButton* buttonInstance = static_cast<xButton*>(data);
    if (buttonInstance) {
        // Declanșăm evenimentul nativ de click prin dispatcherul tău unificat
        buttonInstance->getEventDispatcher().dispatch("click", buttonInstance->getId());
    }
}

void xButton::create(GtkWidget* parent) {
    std::string utf8Text(m_text.begin(), m_text.end());
    
    // Instanțiere widget nativ GTK
    m_widget = gtk_button_new_with_label(utf8Text.c_str());
    
    // Setăm dimensiunea explicită dacă layout-ul o cere
    if (m_width > 0 && m_height > 0) {
        gtk_widget_set_size_request(m_widget, m_width, m_height);
    }

    // 🔥 LEGAREA SEMNALULUI: Înlocuitorul pentru WM_COMMAND / BN_CLICKED
    g_signal_connect(m_widget, "clicked", G_CALLBACK(GtkButtonCallbackAdapter), this);

    // Îl injectăm în părinte (dacă părintele e un GtkFixed sau un GtkBox)
    if (parent) {
        if (GTK_IS_FIXED(parent)) {
            gtk_fixed_put(GTK_FIXED(parent), m_widget, m_x, m_y);
        } else {
            gtk_container_add(GTK_CONTAINER(parent), m_widget);
        }
        gtk_widget_show(m_widget);
    }
}

bool xButton::setProperty(const std::wstring& name, const vData& value) {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    if (prop == L"text") {
        m_text = value.toWString();
        if (m_widget) {
            std::string utf8Text(m_text.begin(), m_text.end());
            gtk_button_set_label(GTK_BUTTON(m_widget), utf8Text.c_str());
        }
        return true;
    }

    return xControl::setProperty(name, value);
}

vData xButton::getProperty(const std::wstring& name) const {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    if (prop == L"text") {
        return vData(m_text);
    }

    return xControl::getProperty(name);
}