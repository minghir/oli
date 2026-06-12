#ifndef XAPP_HPP
#define XAPP_HPP

#include "xWindow.hpp"            // Clasa xWindow adaptată pentru GTK
#include "XWindowManager.hpp"      // Managerul tău de ferestre, acum gestionând xWindow
#include "../../ConsoleManager.hpp"
#include <gtk/gtk.h>              // Include-ul nativ GTK (valabil pe Linux și Windows)
#include <string>
#include <map>
#include <memory>

enum class RunMode { GUI, CONSOLE, SERVICE };

class xApp {
public:
    // În GTK nu există HINSTANCE. Aplicația pornește direct.
    // Putem pasa opțional argumentele din main (argc/argv) dacă vrei să le folosești cu gtk_init.
    explicit xApp(RunMode mode = RunMode::GUI);
    virtual ~xApp() = default;

    // Pornirea buclei grafice (va înlocui bucla ta manuală de mesaje cu gtk_main)
    int run();

    // Inițializarea subsistemelor
    bool init();
    virtual bool initGui();
    virtual bool initConsole() { return true; }
    virtual bool initService() { return true; }

    // 🔥 ARHITECTURĂ GTK: Nu mai avem nevoie de un WndProc static monolithic!
    // În GTK, fiecare widget își gestionează semnalele independent. 
    // Totuși, păstrăm o metodă virtuală dacă vrei să prinzi semnale globale la nivel de aplicație.
    virtual void handleGlobalSignal(GtkWidget* widget, const std::string& signalName);

    static xApp* getAppInstance() { return s_instance; }

    // Returnează GtkWidget* (handle-ul nativ al ferestrei principale, echivalentul HWND)
    GtkWidget* getMainWindow();

    // Gestiunea ferestrelor (Rămâne neschimbată ca logică)
    xWindow* getWindow(const std::string& id);
    void addWindow(const std::string& id, std::unique_ptr<xWindow> window);
    void removeWindow(const std::string& id);
    
    void shutdown();
    void startConsole();
    void test();

    EventDispatcher& getEventDispatcher() { return m_eventDispatcher; }

    void setGlobalVar(const std::wstring& key, const std::wstring& value) { m_globalVars[key] = value; }
    std::wstring getGlobalVar(const std::wstring& key) const {
        auto it = m_globalVars.find(key);
        return (it != m_globalVars.end()) ? it->second : L"";
    }

protected:
    EventDispatcher m_eventDispatcher;
    XWindowManager m_windowManager; // Instanța adaptată pentru xWindow
    RunMode m_runMode = RunMode::GUI;
    std::map<std::wstring, std::wstring> m_globalVars;

private:
    // Pointerul static către instanța unică (Singleton-ul tău curat)
    static xApp* s_instance;
};

#endif // XAPP_HPP