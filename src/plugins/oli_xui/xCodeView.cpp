#include "xCodeView.hpp"
#include <iostream>
#include <cstdlib>

static std::string cv_wstr_to_utf8(const std::wstring& wstr) {
    return std::string(wstr.begin(), wstr.end());
}
static std::wstring cv_utf8_to_wstr(const std::string& str) {
    return std::wstring(str.begin(), str.end());
}

xCodeView::xCodeView(const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher)
    : xPanel(id, x, y, width, height, dispatcher) {}

void xCodeView::create(GtkWidget* parent) {
    // 1. Inițializăm panelul de bază (creează m_widget ca un GtkBox vertical)
    xPanel::create(parent);

    // 2. Adăugăm un Scrolled Window pentru editor în interiorul boxului nostru
    m_scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_scrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    // Forțăm extinderea lui în layout-ul Box
    gtk_box_pack_start(GTK_BOX(m_widget), m_scrolledWindow, TRUE, TRUE, 0);

    // 3. Creăm zona de editare cod text
    m_textView = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(m_textView), GTK_WRAP_NONE); // Codul nu se rupe la capăt de rând
    
    // Setăm un font monospace implicit pentru programare
    PangoFontDescription* fontDesc = pango_font_description_from_string("Monospace 12");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
gtk_widget_override_font(m_textView, fontDesc);
#pragma GCC diagnostic pop
    pango_font_description_free(fontDesc);

    gtk_container_add(GTK_CONTAINER(m_scrolledWindow), m_textView);

    // 4. Conectăm semnalul text-buffer-ului la monitorul de modificări
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_textView));
    g_signal_connect(buffer, "changed", G_CALLBACK(+[](GtkTextBuffer*, gpointer data) {
        auto* self = static_cast<xCodeView*>(data);
        if (self && !self->shouldIgnoreChange()) {
            self->handleTextChanged();
        }
    }), this);

    gtk_widget_show_all(m_widget);
}

void xCodeView::handleTextChanged() {
    m_isDirty = true;
    // 🔥 PROPAGARE: Scriptul Oli a cerut prin .bind("modified") declanșarea acestui eveniment
    m_dispatcher.dispatch("modified", m_id);
}

void xCodeView::setText(const std::wstring& text) {
    if (!m_textView) return;
    
    m_ignoreChange = true; // Blocăm declanșarea asteriscului la încărcarea fișierului
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_textView));
    std::string utf8 = cv_wstr_to_utf8(text);
    gtk_text_buffer_set_text(buffer, utf8.c_str(), -1);
    m_ignoreChange = false;
}

std::wstring xCodeView::getText() const {
    if (!m_textView) return L"";
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_textView));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char* rawText = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    std::string utf8(rawText);
    g_free(rawText);
    return cv_utf8_to_wstr(utf8);
}

void xCodeView::setFontSize(int size) {
    m_fontSize = size;
    if (!m_textView) return;

    // 1. Generăm o regulă CSS dinamică folosind dimensiunea primită
    GtkCssProvider* provider = gtk_css_provider_new();
    std::string css = "textview { font-family: 'Monospace'; font-size: " + std::to_string(size) + "pt; }";
    gtk_css_provider_load_from_data(provider, css.c_str(), -1, NULL);
    
    // 2. Aplicăm providerul CSS pe contextul de stil al widget-ului m_textView
    GtkStyleContext* context = gtk_widget_get_style_context(m_textView);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    // 3. Eliberăm pointerul providerului (contextul îi reține o referință internă)
    g_object_unref(provider);
}

void xCodeView::gotoLine(int lineNum) {
    if (!m_textView) return;
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_textView));
    GtkTextIter iter;
    
    // GTK folosește indexare de la 0 pentru linii
    gtk_text_buffer_get_iter_at_line(buffer, &iter, lineNum - 1);
    gtk_text_buffer_place_cursor(buffer, &iter);
    
    // Scroll automat până la linia cerută pentru a o aduce în centrul ecranului
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(m_textView), &iter, 0.0, TRUE, 0.5, 0.5);
}

void xCodeView::showGoToLineDialog() {
    // Generăm un dialog modal GTK curat
    GtkWidget* dialog = gtk_dialog_new_with_buttons("Sari la Linia...",
        NULL, GTK_DIALOG_MODAL,
        "_Renunță", GTK_RESPONSE_CANCEL,
        "_Sari", GTK_RESPONSE_OK, NULL);
        
    GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* label = gtk_label_new("Introdu numărul liniei:");
    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), "1");

    gtk_container_add(GTK_CONTAINER(content_area), label);
    gtk_container_add(GTK_CONTAINER(content_area), entry);
    gtk_widget_show_all(dialog);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        const char* input = gtk_entry_get_text(GTK_ENTRY(entry));
        int targetLine = std::atoi(input);
        if (targetLine > 0) {
            gotoLine(targetLine);
        }
    }
    gtk_widget_destroy(dialog);
}

// =================================================================
// BINDING-URI SCRIPT OLI
// =================================================================

bool xCodeView::setProperty(const std::wstring& name, const vData& value) {
    if (name == L"file_path") {
        m_currentFilePath = value.toWString();
        return true;
    }
    else if (name == L"syntax_path") {
        m_syntaxPath = value.toWString();
        return true;
    }
    else if (name == L"modified") {
        m_isDirty = value.toBool();
        return true;
    }
    else if (name == L"font_size") {
        this->setFontSize(static_cast<int>(value.toInt()));
        return true;
    }
    else if (name == L"trigger_goto_dialog") {
        if (value.toBool()) {
            this->showGoToLineDialog();
        }
        return true;
    }
    else if (name == L"text") {
        this->setText(value.toWString());
        return true;
    }
    return xPanel::setProperty(name, value);
}

vData xCodeView::getProperty(const std::wstring& name) const {
    if (name == L"file_path") {
        return vData{ m_currentFilePath };
    }
    else if (name == L"syntax_path") {
        return vData{ m_syntaxPath };
    }
    else if (name == L"modified") {
        return vData{ m_isDirty };
    }
    else if (name == L"text") {
        return vData{ this->getText() };
    }
    return xPanel::getProperty(name);
}

bool xCodeView::callMethod(const std::wstring& methodName, const std::vector<vData>& args) {
    if (methodName == L"set_syntax_path" && !args.empty()) {
        m_syntaxPath = args[0].toWString();
        return true;
    }
    else if (methodName == L"set_text" && !args.empty()) {
        this->setText(args[0].toWString());
        return true;
    }
    return xPanel::callMethod(methodName, args);
}