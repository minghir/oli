#include "xControl.hpp"

void xControl::show() {
    m_logicVisible = true;
    if (m_widget) gtk_widget_show(m_widget);
}
void xControl::hide() {
    m_logicVisible = false;
    if (m_widget) gtk_widget_hide(m_widget);
}

void xControl::setFont(const std::wstring& fontName, int baseFontSize, int weight, bool italic, bool underline) {
    m_fontName = fontName; m_baseFontSize = baseFontSize; m_fontWeight = weight;
    std::string ansiName(fontName.begin(), fontName.end());

    if (m_pangoFont) pango_font_description_free(m_pangoFont);
    m_pangoFont = pango_font_description_new();
    pango_font_description_set_family(m_pangoFont, ansiName.c_str());
    pango_font_description_set_size(m_pangoFont, baseFontSize * PANGO_SCALE);
    pango_font_description_set_weight(m_pangoFont, (PangoWeight)weight);
    pango_font_description_set_style(m_pangoFont, italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);

    if (m_widget) {
        gtk_widget_override_font(m_widget, m_pangoFont);
    }
}

void xControl::setBackgroundColor(const std::string& cssColor) {
    m_bgColorCss = cssColor;
    if (!m_widget) return;
    GtkStyleContext *context = gtk_widget_get_style_context(m_widget);
    GtkCssProvider *provider = gtk_css_provider_new();
    std::string css = "* { background-color: " + cssColor + "; }";
    gtk_css_provider_load_from_data(provider, css.c_str(), -1, NULL);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider);
}
