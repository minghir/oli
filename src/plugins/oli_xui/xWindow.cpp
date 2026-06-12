#include "xWindow.hpp"
#include "IXLayoutStrategy.hpp"
#include <algorithm>

// =================================================================
// 🔥 FIX: DEFINIȚIA CONSTRUCTORULUI ȘI A DESTRUCTORULUI
// =================================================================

xWindow::xWindow(const std::string& id, WindowType type, bool isMainWindow, EventDispatcher& dispatcher)
    : xContainer(id, dispatcher), m_isMainWindow(isMainWindow), m_WindowType(type) {
    m_ControlType = ControlType::Window;
}

xWindow::~xWindow() {
    if (m_widget) {
        gtk_widget_destroy(m_widget);
        m_widget = nullptr;
    }
}

// =================================================================
// METODELE DE CREARE ȘI CONTROL GTK
// =================================================================

bool xWindow::create(const std::wstring& /*className*/, const std::wstring& title, unsigned int /*style*/, int x, int y, int w, int h, GtkWidget* parent) {
    // Transformăm titlul din wstring în UTF-8 string pentru GTK
    std::string utf8Title(title.begin(), title.end()); 

    // Creăm fereastra top-level nativă
    m_widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    m_layoutWidget = m_widget; // În testul nostru direct, fereastra acționează ca layout root
    
    gtk_window_set_title(GTK_WINDOW(m_widget), utf8Title.c_str());
    gtk_window_set_default_size(GTK_WINDOW(m_widget), w, h);

    if (parent) {
        gtk_window_set_transient_for(GTK_WINDOW(m_widget), GTK_WINDOW(parent));
    }

    // Centrarea inițială dacă nu se specifică coordonate fixe
    if (x == -1 || y == -1) {
        gtk_window_set_position(GTK_WINDOW(m_widget), GTK_WIN_POS_CENTER);
    }

    // Legăm semnalul de închidere (bătrânul WM_CLOSE devine semnalul "delete-event")
    g_signal_connect(m_widget, "delete-event", G_CALLBACK(+[](GtkWidget* /*w*/, GdkEvent* /*e*/, gpointer data) -> gboolean {
        xWindow* self = static_cast<xWindow*>(data);
        if (self->isMainWindow()) {
            gtk_main_quit();
        }
        return FALSE; // Permite distrugerea widget-ului
    }), this);

    return true;
}

void xWindow::showModal() {
    m_isModal = true;
    gtk_window_set_modal(GTK_WINDOW(m_widget), TRUE);
    this->show();
}

void xWindow::centerWindow() {
    if (m_widget) {
        gtk_window_set_position(GTK_WINDOW(m_widget), GTK_WIN_POS_CENTER);
    }
}

void xWindow::close() {
    if (m_widget) {
        gtk_window_close(GTK_WINDOW(m_widget));
    }
}

bool xWindow::isVisible() const {
    return m_widget ? gtk_widget_get_visible(m_widget) : false;
}

void xWindow::hide() {
    if (m_widget) {
        gtk_widget_hide(m_widget);
    }
}

void xWindow::setIcon(const std::wstring& /*iconPath*/) {
    // Opțional: În GTK setarea icoanei folosește GdkPixbuf. O lăsăm goală momentan pentru test.
}

void xWindow::onSignal(const std::string& /*signalName*/) {
    // Hook pentru semnale interne
}

void xWindow::setMenu(xMenu* /*pMenu*/) {
    // Hook pentru WinMenu -> GtkMenuBar
}

void xWindow::scaleChildren(int /*newDpi*/) {
    // GTK se ocupă nativ de scalare, lăsăm metoda pentru compatibilitate cu scriptul
}

// =================================================================
// PROPRIETĂȚI CERUTE DE INTEGRATION LAYER
// =================================================================

bool xWindow::setProperty(const std::wstring& name, const vData& value) {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    if (prop == L"title") {
        std::wstring wTitle = value.toWString();
        std::string utf8Title(wTitle.begin(), wTitle.end());
        if (m_widget) {
            gtk_window_set_title(GTK_WINDOW(m_widget), utf8Title.c_str());
        }
        return true;
    }
    return xContainer::setProperty(name, value);
}

vData xWindow::getProperty(const std::wstring& name) const {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    if (prop == L"title") {
        if (m_widget) {
            const char* title = gtk_window_get_title(GTK_WINDOW(m_widget));
            std::string sTitle = title ? title : "";
            return vData(std::wstring(sTitle.begin(), sTitle.end()));
        }
    }
    return xContainer::getProperty(name);
}