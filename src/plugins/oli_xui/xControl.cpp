#include "xControl.hpp"
#include <algorithm>

// -----------------------------------------------------------------------------
// CONSTRUCTORI ȘI DESTRUCTOR
// -----------------------------------------------------------------------------

xControl::xControl(const std::string& id, EventDispatcher& dispatcher)
    : m_id(id), m_dispatcher(dispatcher) {
    m_widget = nullptr;
}

xControl::xControl(const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher)
    : m_id(id), m_x(x), m_y(y), m_width(width), m_height(height), m_dispatcher(dispatcher) {
    m_widget = nullptr;
}

xControl::~xControl() {
    if (m_pangoFont) {
        pango_font_description_free(m_pangoFont);
        m_pangoFont = nullptr;
    }
    // Notă: În GTK, widget-urile copil adăugate în containere sunt distruse automat
    // de către managerul de memorie GObject când părintele este eliberat.
    m_children.clear();
}

// -----------------------------------------------------------------------------
// CONTROL VIZIBILITATE ȘI GEOMETRIE
// -----------------------------------------------------------------------------

void xControl::create(GtkWidget* /*parent*/) {
    // Implementare de bază - va fi suprascrisă de clasele derivate
}

void xControl::resize() {
    if (m_widget && m_width > 0 && m_height > 0) {
        gtk_widget_set_size_request(m_widget, m_width, m_height);
    }
}

void xControl::show() {
    m_logicVisible = true;
    if (m_widget) {
        gtk_widget_show(m_widget);
    }
}

void xControl::hide() {
    m_logicVisible = false;
    if (m_widget) {
        gtk_widget_hide(m_widget);
    }
}

bool xControl::isVisible() const {
    return m_widget ? gtk_widget_get_visible(m_widget) : false;
}

void xControl::moveAndResize(int x, int y, int width, int height) {
    m_x = x; m_y = y; m_width = width; m_height = height;
    
    if (m_widget) {
        // 1. Setăm dimensiunea fizică solicitată
        gtk_widget_set_size_request(m_widget, width, height);
        
        // 2. Dacă părintele este un GtkFixed, îl mutăm la coordonatele X, Y absolute
        GtkWidget* parent = gtk_widget_get_parent(m_widget);
        if (parent && GTK_IS_FIXED(parent)) {
            gtk_fixed_move(GTK_FIXED(parent), m_widget, x, y);
        }
    }
}

// -----------------------------------------------------------------------------
// MANAGEMENT IERARHIE COPII (Sincronizat 1:1 cu VM-ul Oli)
// -----------------------------------------------------------------------------

void xControl::addChild(const std::string& id, std::unique_ptr<xControl> ctrl) {
    if (!ctrl) return;
    ctrl->setParent(this);
    m_children.push_back({id, std::move(ctrl)});
}

void xControl::addChild(const std::string& id, std::unique_ptr<xControl> ctrl, GtkWidget* visualParent) {
    if (!ctrl) return;
    ctrl->setParent(this);
    if (visualParent && ctrl->getHandle()) {
        gtk_container_add(GTK_CONTAINER(visualParent), ctrl->getHandle());
    }
    m_children.push_back({id, std::move(ctrl)});
}

xControl* xControl::addChildWithReturn(const std::string& id, std::unique_ptr<xControl> ctrl) {
    if (!ctrl) return nullptr;
    xControl* rawPtr = ctrl.get(); // 🔥 FIX: .get() cere pointerul brut din unique_ptr
    addChild(id, std::move(ctrl));
    return rawPtr;
}

xControl* xControl::getChild(const std::string& id) {
    for (auto& entry : m_children) {
        if (entry.first == id) {
            return entry.second.get();
        }
    }
    return nullptr;
}

void xControl::removeChild(const std::string& id) {
    m_children.erase(
        std::remove_if(m_children.begin(), m_children.end(),
            [&id](const std::pair<std::string, std::unique_ptr<xControl>>& entry) {
                return entry.first == id;
            }),
        m_children.end()
    );
}

std::unique_ptr<xControl> xControl::releaseChild(const std::string& id) {
    for (auto it = m_children.begin(); it != m_children.end(); ++it) {
        if (it->first == id) {
            std::unique_ptr<xControl> released = std::move(it->second);
            m_children.erase(it);
            return released;
        }
    }
    return nullptr;
}

xControl* xControl::getChildRecursive(const std::string& id) {
    xControl* directChild = getChild(id);
    if (directChild) return directChild;

    for (auto& entry : m_children) {
        xControl* found = entry.second->getChildRecursive(id);
        if (found) return found;
    }
    return nullptr;
}

xControl* xControl::findControlByHandle(GtkWidget* widget) {
    if (m_widget == widget) return this;

    for (auto& entry : m_children) {
        xControl* found = entry.second->findControlByHandle(widget);
        if (found) return found;
    }
    return nullptr;
}

void xControl::clearChildren() {
    m_children.clear();
}

// -----------------------------------------------------------------------------
// STILIZARE NATIVĂ (PANGO FONT & GTK CSS PROVIDERS)
// -----------------------------------------------------------------------------

void xControl::setFont(const std::wstring& fontName, int baseFontSize, int weight, bool italic, bool /*underline*/) {
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

void xControl::setFontName(const std::wstring& fontName) {
    setFont(fontName, m_baseFontSize, m_fontWeight, m_fontItalic, m_fontUnderline);
}

void xControl::setFontSize(int baseFontSize) {
    setFont(m_fontName, baseFontSize, m_fontWeight, m_fontItalic, m_fontUnderline);
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

void xControl::setTextColor(const std::string& cssColor) {
    m_textColorCss = cssColor;
    if (!m_widget) return;
    GtkStyleContext *context = gtk_widget_get_style_context(m_widget);
    GtkCssProvider *provider = gtk_css_provider_new();
    std::string css = "* { color: " + cssColor + "; }";
    gtk_css_provider_load_from_data(provider, css.c_str(), -1, NULL);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider);
}

void xControl::setText(const std::wstring& text) {
    if (!m_widget) return;
    std::string utf8Text(text.begin(), text.end());
    // Fallback generic. Metodele derivate (Label, Button, Edit) vor face cast la widgetul lor specific
    if (GTK_IS_LABEL(m_widget)) {
        gtk_label_set_text(GTK_LABEL(m_widget), utf8Text.c_str());
    }
}

void xControl::setTooltipText(const std::wstring& text) {
    if (!m_widget) return;
    std::string utf8Text(text.begin(), text.end());
    gtk_widget_set_tooltip_text(m_widget, utf8Text.c_str());
}

void xControl::setEnabled(bool enable) {
    m_enabled = enable;
    if (m_widget) {
        gtk_widget_set_sensitive(m_widget, enable ? TRUE : FALSE);
    }
}

// -----------------------------------------------------------------------------
// LOGICĂ DE VALIDARE RECURSIVĂ PENTRU FORMULARE
// -----------------------------------------------------------------------------

bool xControl::validateRecursive() {
    bool currentValid = validate();
    for (auto& entry : m_children) {
        if (!entry.second->validateRecursive()) {
            currentValid = false;
        }
    }
    return currentValid;
}

void xControl::onClick() {
    if (m_onClickCallback) {
        m_onClickCallback();
    }
}

bool xControl::setId(const std::string& newId) {
    if (newId.empty()) return false;
    m_id = newId;
    return true;
}

// -----------------------------------------------------------------------------
// INTERFAȚA PROPRIETĂȚILOR VM OLI (`vData` Routing)
// -----------------------------------------------------------------------------

bool xControl::setProperty(const std::wstring& name, const vData& value) {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    if (prop == L"width") {
        setWidth(static_cast<int>(value.toInt()));
        return true;
    }
    if (prop == L"height") {
        setHeight(static_cast<int>(value.toInt()));
        return true;
    }
    if (prop == L"text") {
        setText(value.toWString());
        return true;
    }
    if (prop == L"enabled") {
        setEnabled(value.toBool());
        return true;
    }
    if (prop == L"tooltip") {
        setTooltipText(value.toWString());
        return true;
    }
    if (prop == L"background_color") {
        std::wstring wColor = value.toWString();
        std::string sColor(wColor.begin(), wColor.end());
        setBackgroundColor(sColor);
        return true;
    }
    if (prop == L"text_color") {
        std::wstring wColor = value.toWString();
        std::string sColor(wColor.begin(), wColor.end());
        setTextColor(sColor);
        return true;
    }
    
    
    // 🔥 MAPARE LĂȚIME (width_mode)
        if (name == L"width_mode") {
            std::wstring mode = value.toWString();
            if (mode == L"FILL") {
                gtk_widget_set_hexpand(m_widget, TRUE);
                gtk_widget_set_halign(m_widget, GTK_ALIGN_FILL);
            } else {
                gtk_widget_set_hexpand(m_widget, FALSE);
            }
            return true;
        }

        // 🔥 MAPARE ÎNĂLȚIME (height_mode)
        if (name == L"height_mode") {
            std::wstring mode = value.toWString();
            if (mode == L"FILL") {
                gtk_widget_set_vexpand(m_widget, TRUE);
                gtk_widget_set_valign(m_widget, GTK_ALIGN_FILL);
            } else {
                gtk_widget_set_vexpand(m_widget, FALSE);
            }
            return true;
        }

        // 🔥 MAPARE MARGINI NATIVE (margin)
        if (name == L"margin") {
            int marginSize = static_cast<int>(value.toInt());
            gtk_widget_set_margin_start(m_widget, marginSize);
            gtk_widget_set_margin_end(m_widget, marginSize);
            gtk_widget_set_margin_top(m_widget, marginSize);
            gtk_widget_set_margin_bottom(m_widget, marginSize);
            return true;
        }

        
        
    return false;
}

vData xControl::getProperty(const std::wstring& name) const {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    if (prop == L"width")            return vData(static_cast<long long>(m_width));
    if (prop == L"height")           return vData(static_cast<long long>(m_height));
    if (prop == L"text")             return vData(getText());
    if (prop == L"enabled")          return vData(m_enabled);
    if (prop == L"id")               return vData(std::wstring(m_id.begin(), m_id.end()));
    if (prop == L"margin")           return vData(static_cast<long long>(marginLeft));
    if (prop == L"modified")         return vData(false); // Fallback standard pentru CodeViews
    return vData(std::monostate{});
}