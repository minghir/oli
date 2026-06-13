#include "xTabControl.hpp"
#include "xControl.hpp"
#include "../../PortTools.hpp"
#include <iostream>

// Presupunem că ai o funcție globală sau metodă de căutare a controalelor active
extern xControl* LocateAnyControl(const std::string& id);

xTabControl::xTabControl(const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher)
    : xContainer(id, x, y, width, height, dispatcher) {}

void xTabControl::create(GtkWidget* parent) {
    // 1. GtkNotebook este echivalentul perfect al lui TabControl din Win32
    m_widget = gtk_notebook_new();
    gtk_widget_set_name(m_widget, m_id.c_str());
    
    // Permitem scroll orizontal dacă sunt deschise foarte multe fișiere/tab-uri
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(m_widget), TRUE);

    // 2. Dimensiuni
    gtk_widget_set_size_request(m_widget, m_width, m_height);

    // 3. Atașare la părinte
    if (parent && GTK_IS_CONTAINER(parent)) {
        gtk_container_add(GTK_CONTAINER(parent), m_widget);
    }

    // 4. Conectăm semnalul de schimbare a tab-ului pentru evenimentul "tab_changed"
    g_signal_connect(m_widget, "switch-page", G_CALLBACK(+[](GtkNotebook* /*notebook*/, GtkWidget* /*page*/, guint page_num, gpointer data) {
        auto* self = static_cast<xTabControl*>(data);
        // Anunțăm scriptul că utilizatorul a schimbat tab-ul activ
        self->getEventDispatcher().dispatch("tab_changed", self->getId());
    }), this);

    gtk_widget_show(m_widget);
}

int xTabControl::findPageIndexById(const std::string& pageId) const {
    for (size_t i = 0; i < m_pages.size(); ++i) {
        if (m_pages[i].id == pageId) return static_cast<int>(i);
    }
    return -1;
}

bool xTabControl::setProperty(const std::wstring& name, const vData& value) {
    return xContainer::setProperty(name, value);
}

vData xTabControl::getProperty(const std::wstring& name) const {
    if (name == L"active_tab_id") {
        int currentIdx = gtk_notebook_get_current_page(GTK_NOTEBOOK(m_widget));
        if (currentIdx >= 0 && currentIdx < static_cast<int>(m_pages.size())) {
            return vData{ PortTools::utf8_to_wstring(m_pages[currentIdx].id) };
        }
        return vData{ L"" };
    }
    else if (name == L"tab_count") {
        return vData{ static_cast<long long>(m_pages.size()) };
    }

    return xContainer::getProperty(name);
}

bool xTabControl::callMethod(const std::wstring& methodName, const std::vector<vData>& args) {
    if (methodName == L"add_tab" && args.size() >= 2) {
        std::wstring wTitle = args[0].toWString();
        std::wstring wPageId = args[1].toWString();
        std::string pageId(wPageId.begin(), wPageId.end());

        xControl* ctrl = LocateAnyControl(pageId);
        if (!ctrl || !ctrl->getHandle()) return false;

        GtkWidget* childWidget = ctrl->getHandle();

        // Construim un header de tab custom: un GtkBox ce conține un Label și un mic buton Close (X)
        GtkWidget* tabBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        std::string utf8Title = PortTools::wstring_to_utf8(wTitle);
        GtkWidget* label = gtk_label_new(utf8Title.c_str());
        GtkWidget* closeBtn = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU);
        
        // Curățăm aspectul butonului de X să fie plat/fără margini groase
        gtk_button_set_relief(GTK_BUTTON(closeBtn), GTK_RELIEF_NONE);

        gtk_box_pack_start(GTK_BOX(tabBox), label, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(tabBox), closeBtn, FALSE, FALSE, 0);
        gtk_widget_show_all(tabBox);

        // Deoarece în factory-ul din `oli_xui.cpp` copilul a rulat deja `gtk_container_add` pe notebook,
        // el este deja înregistrat ca pagină brută. Noi doar îi customizăm și îi aplicăm header-ul vizual.
        gtk_notebook_set_tab_label(GTK_NOTEBOOK(m_widget), childWidget, tabBox);

        // Conectăm butonul de X la evenimentul "close_tab" cerut de script
        std::string* heapId = new std::string(pageId);
        g_object_set_data_full(G_OBJECT(closeBtn), "target_page_id", heapId, [](gpointer data) {
            delete static_cast<std::string*>(data);
        });

        g_signal_connect(closeBtn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer data) {
            auto* self = static_cast<xTabControl*>(data);
            std::string* pId = static_cast<std::string*>(g_object_get_data(G_OBJECT(btn), "target_page_id"));
            if (pId) {
                // Trimitem semnalul către funcția `on_tab_close_requested` din script
                self->getEventDispatcher().dispatch("close_tab", *pId);
            }
        }), this);

        // Înregistrăm tab-ul în vectorul nostru de tracking
        m_pages.push_back({ pageId, childWidget, tabBox, label });
        return true;
    }
    else if (methodName == L"switch_page" && !args.empty()) {
        int idx = static_cast<int>(args[0].toInt());
        gtk_notebook_set_current_page(GTK_NOTEBOOK(m_widget), idx);
        return true;
    }
    else if (methodName == L"select_tab_by_id" && !args.empty()) {
        std::wstring wPageId = args[0].toWString();
        std::string pageId(wPageId.begin(), wPageId.end());
        
        int idx = findPageIndexById(pageId);
        if (idx != -1) {
            gtk_notebook_set_current_page(GTK_NOTEBOOK(m_widget), idx);
            return true;
        }
        return false;
    }
    else if (methodName == L"set_title_by_id" && args.size() >= 2) {
        std::wstring wPageId = args[0].toWString();
        std::string pageId(wPageId.begin(), wPageId.end());
        std::wstring wNewTitle = args[1].toWString();

        int idx = findPageIndexById(pageId);
        if (idx != -1) {
            std::string utf8Title = PortTools::wstring_to_utf8(wNewTitle);
            gtk_label_set_text(GTK_LABEL(m_pages[idx].labelWidget), utf8Title.c_str());
            return true;
        }
        return false;
    }
    else if (methodName == L"remove_tab_page_by_id" && !args.empty()) {
        std::wstring wPageId = args[0].toWString();
        std::string pageId(wPageId.begin(), wPageId.end());

        int idx = findPageIndexById(pageId);
        if (idx != -1) {
            // Eliminăm widget-ul fizic din structura GtkNotebook
            gtk_notebook_remove_page(GTK_NOTEBOOK(m_widget), idx);
            // Îl scoatem și din vectorul nostru de monitorizare
            m_pages.erase(m_pages.begin() + idx);
            return true;
        }
        return false;
    }
    else if (methodName == L"refresh") {
        gtk_widget_queue_draw(m_widget);
        return true;
    }

    return xContainer::callMethod(methodName, args);
}