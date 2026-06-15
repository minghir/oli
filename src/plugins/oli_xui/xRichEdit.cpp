#include "xRichEdit.hpp"
#include <iostream>

// Helperi pentru conversie locală de string-uri (asemanatori cu cei din plugin)
static std::string rich_wstr_to_utf8(const std::wstring& wstr) {
    return std::string(wstr.begin(), wstr.end());
}
static std::wstring rich_utf8_to_wstr(const std::string& str) {
    return std::wstring(str.begin(), str.end());
}

xRichEdit::xRichEdit(const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher)
    : xControl(id, x, y, width, height, dispatcher) {}

void xRichEdit::create(GtkWidget* parent) {
    // 1. Containerul principal este un Scrolled Window pentru barele de defilare
    m_widget = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_name(m_widget, m_id.c_str());
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_widget), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(m_widget, m_width, m_height);

    // 2. Widget-ul intern de editare text
    m_textView = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(m_textView), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(m_widget), m_textView);

    // 3. Atașare la părinte
    if (parent && GTK_IS_CONTAINER(parent)) {
        gtk_container_add(GTK_CONTAINER(parent), m_widget);
    }

    // 4. Activăm evenimentele de structură și mouse pe container pentru resize dinamic
    gtk_widget_add_events(m_widget, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

    g_signal_connect(m_widget, "button-press-event", G_CALLBACK(+[](GtkWidget* /*w*/, GdkEventButton* event, gpointer data) -> gboolean {
        static_cast<xRichEdit*>(data)->handleButtonPress(event);
        return FALSE; // Permitem propagarea mai departe
    }), this);

    g_signal_connect(m_widget, "button-release-event", G_CALLBACK(+[](GtkWidget* /*w*/, GdkEventButton* event, gpointer data) -> gboolean {
        static_cast<xRichEdit*>(data)->handleButtonRelease(event);
        return FALSE;
    }), this);

    g_signal_connect(m_widget, "motion-notify-event", G_CALLBACK(+[](GtkWidget* /*w*/, GdkEventMotion* event, gpointer data) -> gboolean {
        static_cast<xRichEdit*>(data)->handleMotionNotify(event);
        return FALSE;
    }), this);

    gtk_widget_show_all(m_widget);
}

void xRichEdit::setText(const std::wstring& text) {
    if (!m_textView) return;
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_textView));
    std::string utf8 = rich_wstr_to_utf8(text);
    gtk_text_buffer_set_text(buffer, utf8.c_str(), -1);
}

std::wstring xRichEdit::getText() const {
    if (!m_textView) return L"";
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_textView));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char* rawText = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    std::string utf8(rawText);
    g_free(rawText);
    return rich_utf8_to_wstr(utf8);
}

void xRichEdit::appendText(const std::wstring& text) {
    if (!m_textView) return;
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_textView));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    std::string utf8 = rich_wstr_to_utf8(text);
    gtk_text_buffer_insert(buffer, &end, utf8.c_str(), -1);

    // Auto-scroll la finalul consolei
    gtk_text_buffer_get_end_iter(buffer, &end);
    GtkTextMark* mark = gtk_text_buffer_create_mark(buffer, NULL, &end, FALSE);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(m_textView), mark);
    gtk_text_buffer_delete_mark(buffer, mark);
}

void xRichEdit::setReadOnly(bool readOnly) {
    m_isReadOnly = readOnly;
    if (m_textView) {
        gtk_text_view_set_editable(GTK_TEXT_VIEW(m_textView), !readOnly);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(m_textView), !readOnly);
    }
}

void xRichEdit::setFontSize(int size) {
    if (!m_textView) return;
    PangoFontDescription* fontDesc = pango_font_description_new();
    pango_font_description_set_size(fontDesc, size * PANGO_SCALE);
    gtk_widget_override_font(m_textView, fontDesc);
    pango_font_description_free(fontDesc);
}

// =================================================================
// LOGICĂ DE RESIZE PRIN EVENIMENTE DE MOUSE GTK
// =================================================================

void xRichEdit::handleButtonPress(GdkEventButton* event) {
    if (!m_isResizable || event->button != 1) return;

    // Verificăm dacă mouse-ul a apăsat aproape de marginea de sus a consolei (y < m_resizeMargin)
    if (event->y < m_resizeMargin) {
        m_isResizing = true;
        m_lastMouseY = static_cast<int>(event->y_root);
        
        // Schimbăm cursorul ferestrei într-unul de redimensionare verticală
        GdkWindow* window = gtk_widget_get_window(m_widget);
        GdkCursor* cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_SB_V_DOUBLE_ARROW);
        gdk_window_set_cursor(window, cursor);
        g_object_unref(cursor);
    }
}

void xRichEdit::handleButtonRelease(GdkEventButton* event) {
    if (event->button == 1 && m_isResizing) {
        m_isResizing = false;
        // Resetăm cursorul la cel normal
        GdkWindow* window = gtk_widget_get_window(m_widget);
        gdk_window_set_cursor(window, NULL);
    }
}

void xRichEdit::handleMotionNotify(GdkEventMotion* event) {
    if (!m_isResizable) return;

    GdkWindow* window = gtk_widget_get_window(m_widget);

    // Feedback vizual: Schimbăm cursorul când mouse-ul doar plutește peste marginea de sus
    if (!m_isResizing) {
        if (event->y < m_resizeMargin) {
            GdkCursor* cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_SB_V_DOUBLE_ARROW);
            gdk_window_set_cursor(window, cursor);
            g_object_unref(cursor);
        } else {
            gdk_window_set_cursor(window, NULL);
        }
        return;
    }

    // Calculăm delta exactă cerută de scriptul Oli
    int currentY = static_cast<int>(event->y_root);
    m_resizeDelta = currentY - m_lastMouseY;
    m_lastMouseY = currentY;

    // 🔥 Propagăm semnalul în script
    if (m_resizeDelta != 0) {
        m_dispatcher.dispatch("RESIZE_VERTICAL", m_id);
    }
}

// =================================================================
// INTEGRAREA ÎN ENGINE-UL OLI
// =================================================================

bool xRichEdit::setProperty(const std::wstring& name, const vData& value) {
    if (name == L"text") {
        this->setText(value.toWString());
        return true;
    }
    else if (name == L"read_only") {
        this->setReadOnly(value.toBool());
        return true;
    }
    else if (name == L"font_size") {
        this->setFontSize(static_cast<int>(value.toInt()));
        return true;
    }
    else if (name == L"resizable") {
        m_isResizable = value.toBool();
        return true;
    }
    return xControl::setProperty(name, value);
}

vData xRichEdit::getProperty(const std::wstring& name) const {
    if (name == L"text") {
        return vData{ this->getText() };
    }
    else if (name == L"resize_delta") {
        // Scriptul citește valoarea de aici în interiorul callback-ului 'on_console_resize'
        return vData{ static_cast<long long>(m_resizeDelta) };
    }
    else if (name == L"height") {
        return vData{ static_cast<long long>(m_height) };
    }
    return xControl::getProperty(name);
}

bool xRichEdit::callMethod(const std::wstring& methodName, const std::vector<vData>& args) {
    if (methodName == L"append_text" && !args.empty()) {
        this->appendText(args[0].toWString());
        return true;
    }
    else if (methodName == L"set_text" && !args.empty()) {
        this->setText(args[0].toWString());
        return true;
    }
    else if (methodName == L"set_read_only" && !args.empty()) {
        this->setReadOnly(args[0].toBool());
        return true;
    }
    else if (methodName == L"set_font_size" && !args.empty()) {
        this->setFontSize(static_cast<int>(args[0].toInt()));
        return true;
    }
    else if (methodName == L"set_resizable" && !args.empty()) {
        m_isResizable = args[0].toBool();
        return true;
    }
    return xControl::callMethod(methodName, args);
}