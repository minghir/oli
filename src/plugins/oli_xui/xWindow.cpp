#include "xWindow.hpp"
#include "IXLayoutStrategy.hpp"
#include <algorithm>

bool xWindow::create(const std::wstring& /*className*/, const std::wstring& title, unsigned int /*style*/, int x, int y, int w, int h, GtkWidget* parent) {
    // restul codului rămâne neschimbat... {
    // Transformăm titlul din wstring în UTF-8 string pentru GTK
    std::string utf8Title(title.begin(), title.end()); 

    // Creăm fereastra top-level
    m_widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    
    gtk_window_set_title(GTK_WINDOW(m_widget), utf8Title.c_str());
    gtk_window_set_default_size(GTK_WINDOW(m_widget), w, h);

    if (parent) {
        gtk_window_set_transient_for(GTK_WINDOW(m_widget), GTK_WINDOW(parent));
    }

    // Centrarea inițială dacă nu se specifică coordonate fixe
    if (x == -1 || y == -1) {
        gtk_window_set_position(GTK_WINDOW(m_widget), GTK_WIN_POS_CENTER);
    } else {
        // Notă: GTK preferă layout structural, dar suportă și poziționare fixă
    }

    // 🔥 Legăm semnalul de închidere (bătrânul WM_CLOSE devine semnalul "delete-event")
    g_signal_connect(m_widget, "delete-event", G_CALLBACK(+[](GtkWidget* /*w*/, GdkEvent* /*e*/, gpointer data) -> gboolean {
        xWindow* self = static_cast<xWindow*>(data);
        if (self->isMainWindow()) {
            gtk_main_quit();
        }
        return FALSE;
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