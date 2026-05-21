#include "../../OliEngine.hpp"
#include "ui/vApp.hpp"
#include "ui/vWindow.hpp"
#include "ui/vPanel.hpp"
#include "ui/vCodeView.hpp"
#include <unordered_map>
#include <memory>

#ifdef _WIN32
#define OLI_EXPORT extern "C" __declspec(dllexport)
#else
#define OLI_EXPORT extern "C"
#endif

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

// Structură pentru a păstra referințele ferestrelor create din script
struct GuiState {
    std::unique_ptr<vApp> ownedAppInstance = nullptr; // Deține memoria dacă îl creăm noi
    vApp* appInstance = nullptr;                      // Pointerul de lucru (folosit peste tot)
    bool isInitialized = false;
} g_Gui;

OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {

    // --- GUI_INIT() ---
    // Inițializează instanța vApp pentru plugin
    registry[L"GUI_INIT"] = [](const std::vector<vData>&) -> vData {
        if (g_Gui.isInitialized) return vData{ 1LL };

        // Încercăm mai întâi să vedem dacă aplicația gazdă are deja un vApp pornit
        g_Gui.appInstance = vApp::getAppInstance();

        // Dacă NU există (rulam dintr-un shell de consolă), îl creăm noi pe loc!
        if (!g_Gui.appInstance) {
            // Preluăm handle-ul instanței modulului curent/principal
            HINSTANCE hInst = GetModuleHandle(nullptr);
            
            // Instanțiem vApp-ul nostru în mod grafic (RunMode::GUI)
            g_Gui.ownedAppInstance = std::make_unique<vApp>(hInst, RunMode::GUI);
            g_Gui.appInstance = g_Gui.ownedAppInstance.get();

            // Apelăm funcția de inițializare internă a framework-ului (înregistrare clase window, etc.)
            if (!g_Gui.appInstance->init()) {
                g_Gui.ownedAppInstance.reset();
                g_Gui.appInstance = nullptr;
                return vData{ 0LL }; // Inițializarea WinAPI a eșuat
            }
        }

        g_Gui.isInitialized = true;
        return vData{ 1LL };
    };

    // --- WIN_CREATE(id, titlu) -> returns success ---
    // Creează o fereastră vWindow standard din script
    registry[L"WIN_CREATE"] = [](const std::vector<vData>& args) -> vData {
        if (!g_Gui.isInitialized || args.size() < 2) return vData{ 0LL };

        std::wstring wId = std::get<std::wstring>(args[0].value);
        std::wstring wTitle = std::get<std::wstring>(args[1].value);
        
        // Conversie id din wstring în string pentru framework-ul tău
        std::string id(wId.begin(), wId.end());

        HINSTANCE hInst = g_Gui.appInstance->getInstance();
        auto newWindow = std::make_unique<vWindow>(
            hInst, 
            id, 
            WindowType::StandardWindow, 
            false, 
            g_Gui.appInstance->getEventDispatcher()
        );
		newWindow->setIsMainWindow(true);
        // Creare fereastră reală Windows (HWND)
        bool created = newWindow->create(L"VOliWindowClass", wTitle, 
                                        WS_OVERLAPPEDWINDOW, 200, 200, 800, 600, 
                                        nullptr, nullptr);

        if (!created) return vData{ 0LL };

        // O adăugăm în managerul central din vApp
        g_Gui.appInstance->addWindow(id, std::move(newWindow));
        return vData{ 1LL };
    };

    // --- WIN_SHOW(id) ---
    // Afișează o fereastră înregistrată pe ecran
    registry[L"WIN_SHOW"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData{ 0LL };
        
        std::wstring wId = std::get<std::wstring>(args[0].value);
        std::string id(wId.begin(), wId.end());

        vWindow* win = g_Gui.appInstance->getWindow(id);
        if (win) {
            win->show();
            return vData{ 1LL };
        }
        return vData{ 0LL };
    };
	
	registry[L"GUI_RUN"] = [](const std::vector<vData>&) -> vData {
        if (!g_Gui.isInitialized || !g_Gui.appInstance) {
            return vData{ 0LL };
        }

        // Pornim bucla principală din vApp. 
        // Aceasta va rula până când se primește WM_QUIT (de exemplu, când închizi fereastra)
        int exitCode = g_Gui.appInstance->run();

        return vData{ static_cast<long long>(exitCode) };
    };
}