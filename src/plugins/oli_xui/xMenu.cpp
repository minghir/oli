#include "../../StringUtils.hpp"
#include "../../PortTools.hpp"
#include "xMenu.hpp"

// Declarăm extern funcția din oli_xui.cpp pentru a putea localiza submeniurile din controlsMap
extern xControl* LocateAnyControl(const std::string& id);

xMenu::xMenu(const std::string& id, EventDispatcher& dispatcher)
    : xControl(id, 0, 0, 0, 0, dispatcher), m_menuWidget(gtk_menu_new()) {}

void xMenu::create(GtkWidget* parent) {
    if (!parent) return;

    // 🔥 FIX VIZUALIZARE: Dacă părintele este bara de meniu (GtkMenuBar)
    if (GTK_IS_MENU_BAR(parent)) {
        // Creăm elementul vizibil de pe bară. Folosim ID-ul sau un text generic.
        std::string labelText = m_id;
        if (labelText == "main_menu" || labelText == "menu_principal") {
            labelText = "Meniu";
        } else if (labelText.rfind("menu_", 0) == 0) {
            labelText = labelText.substr(5); // elimină prefixul "menu_" pentru un aspect curat (ex: "file")
            if(!labelText.empty()) labelText[0] = toupper(labelText[0]);
        }

        GtkWidget* rootItem = gtk_menu_item_new_with_label(labelText.c_str());
        
        // Conectăm m_menuWidget (dropdown-ul) la acest buton de pe bară
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(rootItem), m_menuWidget);
        
        // Împingem butonul în bară
        gtk_menu_shell_append(GTK_MENU_SHELL(parent), rootItem);
        
        // Forțăm afișarea widget-ului rădăcină
        gtk_widget_show(rootItem);
    }
}

void xMenu::addItem(const std::string& id, const std::wstring& text) {
    std::string utf8Text = PortTools::wstring_to_utf8(text);
    
    // Curățăm scurtăturile de tip Win32 (\tCtrl+N) pentru a nu urâți textul în GTK
    size_t tabPos = utf8Text.find('\t');
    if (tabPos != std::string::npos) {
        utf8Text = utf8Text.substr(0, tabPos);
    }

    GtkWidget* item = gtk_menu_item_new_with_label(utf8Text.c_str());
    
    g_signal_connect(item, "activate", G_CALLBACK(+[](GtkWidget* /*w*/, gpointer data) {
        auto* menu = static_cast<xMenu*>(data);
        menu->getEventDispatcher().dispatch("click", menu->getId()); 
    }), this);

    gtk_menu_shell_append(GTK_MENU_SHELL(m_menuWidget), item);
    m_menuItems.push_back({id, text, item, false});
    gtk_widget_show(item);
}

void xMenu::addSeparator(const std::string& id) {
    GtkWidget* sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(m_menuWidget), sep);
    m_menuItems.push_back({id, L"", sep, true});
    gtk_widget_show(sep);
}

void xMenu::addSubMenu(const std::wstring& text, xMenu* subMenu) {
    // 🔥 FIX COMPILARE: Schimbat în PortTools::wstring_to_utf8 pentru consistență
    std::string utf8Text = PortTools::wstring_to_utf8(text);
    GtkWidget* item = gtk_menu_item_new_with_label(utf8Text.c_str());
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), subMenu->getHandle());
    gtk_menu_shell_append(GTK_MENU_SHELL(m_menuWidget), item);
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
    // 🔥 FIX METODĂ: Adăugat suport complet pentru separatoare din script
    else if (methodName == L"add_separator") {
        if (args.size() >= 1) {
            std::wstring wId = args[0].toWString();
            std::string id(wId.begin(), wId.end());
            this->addSeparator(id);
            return true;
        }
    }
    // 🔥 FIX METODĂ: Adăugat suport pentru submeniuri recursive (File -> New, etc.)
    else if (methodName == L"add_submenu") {
        if (args.size() >= 2) {
            std::wstring text = args[0].toWString();
            std::wstring wSubMenuId = args[1].toWString();
            std::string subMenuId(wSubMenuId.begin(), wSubMenuId.end());
            
            // Căutăm submeniul în map-ul global utilizând ID-ul primit din script
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