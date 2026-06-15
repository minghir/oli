#include "../../StringUtils.hpp"
#include "../../PortTools.hpp"
#include "xMenu.hpp"

extern xControl* LocateAnyControl(const std::string& id);

xMenu::xMenu(const std::string& id, EventDispatcher& dispatcher)
    : xControl(id, 0, 0, 0, 0, dispatcher), 
      m_menuWidget(gtk_menu_new()), 
      m_menuBarWidget(nullptr), 
      m_isMenuBar(false) {}

void xMenu::create(GtkWidget* parent) {
    if (!parent) return;

    // 🔥 FIX TOP-LEVEL: Dacă părintele este GtkMenuBar, reținem containerul principal
    if (GTK_IS_MENU_BAR(parent)) {
        m_menuBarWidget = parent;
        m_isMenuBar = true;
        // Nu mai creăm un rootItem "Meniu" intermediar!
    }
}

void xMenu::addItem(const std::string& id, const std::wstring& text) {
    std::string utf8Text = PortTools::wstring_to_utf8(text);
    
    size_t tabPos = utf8Text.find('\t');
    if (tabPos != std::string::npos) {
        utf8Text = utf8Text.substr(0, tabPos);
    }

    GtkWidget* item = gtk_menu_item_new_with_label(utf8Text.c_str());
    
    g_signal_connect(item, "activate", G_CALLBACK(+[](GtkWidget* /*w*/, gpointer data) {
        auto* menu = static_cast<xMenu*>(data);
        menu->getEventDispatcher().dispatch("click", menu->getId()); 
    }), this);

    // 🔥 Dacă suntem pe bara principală, adăugăm direct acolo
    if (m_isMenuBar && m_menuBarWidget) {
        gtk_menu_shell_append(GTK_MENU_SHELL(m_menuBarWidget), item);
    } else {
        gtk_menu_shell_append(GTK_MENU_SHELL(m_menuWidget), item);
    }

    m_menuItems.push_back({id, text, item, false});
    gtk_widget_show(item);
}

void xMenu::addSeparator(const std::string& id) {
    GtkWidget* sep = gtk_separator_menu_item_new();
    
    if (m_isMenuBar && m_menuBarWidget) {
        gtk_menu_shell_append(GTK_MENU_SHELL(m_menuBarWidget), sep);
    } else {
        gtk_menu_shell_append(GTK_MENU_SHELL(m_menuWidget), sep);
    }

    m_menuItems.push_back({id, L"", sep, true});
    gtk_widget_show(sep);
}

void xMenu::addSubMenu(const std::wstring& text, xMenu* subMenu) {
    std::string utf8Text = PortTools::wstring_to_utf8(text);
    GtkWidget* item = gtk_menu_item_new_with_label(utf8Text.c_str());
    
    // Conectăm dropdown-ul submeniului (ex: fileMenu) la butonul de pe bară (ex: "File")
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), subMenu->getHandle());
    
    // 🔥 Împingem butonul direct în bara orizontală sau în meniul părinte
    if (m_isMenuBar && m_menuBarWidget) {
        gtk_menu_shell_append(GTK_MENU_SHELL(m_menuBarWidget), item);
    } else {
        gtk_menu_shell_append(GTK_MENU_SHELL(m_menuWidget), item);
    }
    
    gtk_widget_show(item);
}

bool xMenu::setProperty(const std::wstring& name, const vData& value) {
    return xControl::setProperty(name, value);
}

vData xMenu::getProperty(const std::wstring& name) const {
    return xControl::getProperty(name);
}

bool xMenu::callMethod(const std::wstring& methodName, const std::vector<vData>& args) {
    if (methodName == L"add_item") {
        if (args.size() >= 2) {
            std::wstring wId = args[0].toWString(); 
            std::string id(wId.begin(), wId.end()); 
            std::wstring text = args[1].toWString();
            
            this->addItem(id, text);
            return true;
        }
    }
    else if (methodName == L"add_separator") {
        if (args.size() >= 1) {
            std::wstring wId = args[0].toWString();
            std::string id(wId.begin(), wId.end());
            this->addSeparator(id);
            return true;
        }
    }
    else if (methodName == L"add_submenu") {
        if (args.size() >= 2) {
            std::wstring text = args[0].toWString();
            std::wstring wSubMenuId = args[1].toWString();
            std::string subMenuId(wSubMenuId.begin(), wSubMenuId.end());
            
            xControl* ctrl = LocateAnyControl(subMenuId);
            xMenu* subMenuPtr = dynamic_cast<xMenu*>(ctrl);
            
            if (subMenuPtr) {
                this->addSubMenu(text, subMenuPtr);
                return true;
            }
        }
    }
    return xControl::callMethod(methodName, args);
}