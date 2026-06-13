#include "../../OliEngine.hpp"
#include "../../IOliEngine.hpp"
#include "xApp.hpp"
#include "xWindow.hpp"
#include "xContainer.hpp"
#include "xButton.hpp"
#include "xMenu.hpp"
#include "xPanel.hpp"
#include "xTabControl.hpp"
#include "IXLayoutStrategy.hpp"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <gtk/gtk.h>
#include <locale>
#include <codecvt>

#ifdef _WIN32
#define OLI_EXPORT extern "C" __declspec(dllexport)
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#endif



static std::string wstr_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    try {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.to_bytes(wstr);
    } catch (...) {
        return std::string(wstr.begin(), wstr.end());
    }
}

static std::wstring utf8_to_wstr(const std::string& str) {
    return ::str_to_wstr(str); // Va apela variabila din StringUtils.o
}

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

struct GuiState {
    std::unique_ptr<xApp> ownedAppInstance = nullptr;
    xApp* appInstance = nullptr;
    bool isInitialized = false;
    std::unordered_map<std::string, xControl*> controlsMap;
} g_XuiGui;

inline vOliEngine* g_LinkedOliEngine = nullptr;
static std::map<std::string, std::wstring> g_PersistentScriptCallbacks;

xControl* LocateAnyControl(const std::string& id) {
    if (!g_XuiGui.appInstance) return nullptr;
    auto it = g_XuiGui.controlsMap.find(id);
    if (it != g_XuiGui.controlsMap.end()) {
        return it->second;
    }
    return nullptr;
}

// ✅ PUNCTUL UNIC DE INTRARE ÎN PLUGIN
OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry, void* enginePtr) {

    if (g_LinkedOliEngine == nullptr) {
        g_LinkedOliEngine = static_cast<vOliEngine*>(enginePtr);
        if (g_LinkedOliEngine) {
            LOG_SUCCESS(L"✅ [oli_xui] Conexiune stabilă stabilită cu OliEngine-ul Gazdă!");
        } else {
            LOG_ERROR(L"❌ [oli_xui] Eroare critică: Am primit un pointer enginePtr de tip NULL!");
        }
    } else {
        LOG_INFO(L"ℹ️ [oli_xui] LoadOliPlugin re-apelat. Păstrăm engine-ul gazdă intact.");
    }

    // ==========================================
    // 1. COMPONENTA: XAPP / WINAPP
    // ==========================================

    registry[L"CREATE_XAPP"] = [](const std::vector<vData>&) -> vData {
        auto objMap = std::make_shared<std::unordered_map<std::wstring, vData>>();
        (*objMap)[L"__type__"] = vData(L"WinApp");
        return vData(objMap);
    };

    registry[L"XAPP::INIT"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData{ 0LL };
        if (g_XuiGui.isInitialized) return vData{ 1LL };

        g_XuiGui.appInstance = xApp::getAppInstance();

        if (!g_XuiGui.appInstance) {
            g_XuiGui.ownedAppInstance = std::make_unique<xApp>(RunMode::GUI);
            g_XuiGui.appInstance = g_XuiGui.ownedAppInstance.get();

            if (!g_XuiGui.appInstance->init()) {
                // 🔥 ADAGĂ ACEST LOG PENTRU CORECTITUDINE:
                LOG_ERROR(L"❌ [XAPP::INIT] gtk_init_check a esuat! Lipseste variabila DISPLAY sau serverul X11.");
                g_XuiGui.ownedAppInstance.reset();
                g_XuiGui.appInstance = nullptr;
                return vData{ 0LL };
            }
        }

        g_XuiGui.isInitialized = true;
        return vData{ 1LL };
    };

    registry[L"XAPP::RUN"] = [](const std::vector<vData>&) -> vData {
        if (!g_XuiGui.isInitialized || !g_XuiGui.appInstance) {
            return vData{ 0LL };
        }
        int exitCode = g_XuiGui.appInstance->run();
        return vData{ static_cast<long long>(exitCode) };
    };

    // ==========================================
    // 2. COMPONENTA: XWINDOW / WINWINDOW
    // ==========================================

    registry[L"CREATE_XWINDOW"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{ std::monostate{} };

        try {
            std::wstring wId = args[0].toWString();
            std::wstring wTitle = args[1].toWString();
            std::string id(wId.begin(), wId.end());

            LOG_DEBUG(L"[oli_xui] Fereastră GTK: " + wId + L" | Titlu: " + wTitle);

            auto objMap = std::make_shared<std::unordered_map<std::wstring, vData>>();
            (*objMap)[L"__type__"] = vData(L"WinWindow");
            (*objMap)[L"id"] = vData(wId);
            (*objMap)[L"title"] = vData(wTitle);

            xApp* actualApp = xApp::getAppInstance();
            if (!actualApp) {
                LOG_ERROR(L"❌ [CREATE_WINWINDOW] appInstance este NULL!");
                return vData{ std::monostate{} };
            }

            auto newWindow = std::make_unique<xWindow>(
                id, WindowType::StandardWindow, true, actualApp->getEventDispatcher()
            );
            newWindow->setIsMainWindow(true);

            bool created = newWindow->create(L"XOliWindowClass", wTitle, 0, 200, 200, 800, 600, nullptr);

            if (created) {
                xWindow* winPtr = newWindow.get();
                actualApp->addWindow(id, std::move(newWindow));
                g_XuiGui.controlsMap[id] = winPtr;
                LOG_DEBUG(L"[oli_xui] Fereastra GTK a fost înregistrată structural.");
            } else {
                LOG_ERROR(L"❌ [CREATE_WINWINDOW] Crearea GtkWindow a eșuat!");
            }

            return vData(objMap);
        }
        catch (...) {
            LOG_ERROR(L"❌ [CREATE_WINWINDOW] Excepție la crearea ferestrei GTK!");
            return vData{ std::monostate{} };
        }
    };

    registry[L"XWINDOW::SHOW"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData{ 0LL };
        vData self = args[0];

        if (!self.isMap()) return vData{ 0LL };
        auto m = self.rawMap();

        std::wstring wId = (*m)[L"id"].toWString();
        std::string id(wId.begin(), wId.end());

        xControl* ctrl = LocateAnyControl(id);
        if (ctrl) {
            ctrl->show();
            return vData{ 1LL };
        }
        return vData{ 0LL };
    };

    // =================================================================
    // 3. CONSTRUCTORUL GENERIC DE CONTROALE VIZUALE
    // =================================================================

    registry[L"UI_CREATE_CONTROL"] = [](const std::vector<vData>& args) -> vData {
        LOG_DEBUG(L"[XUI_DEBUG] Intrare UI_CREATE_CONTROL. Tip: " + (args.size() > 0 ? args[0].toWString() : L"N/A"));

        if (args.size() < 3) return vData{ 0LL };

        std::wstring wType = args[0].toWString();
        std::wstring wId = args[1].toWString();
        std::wstring wParentId = args[2].toWString();

        std::string id(wId.begin(), wId.end());
        std::string parentId(wParentId.begin(), wParentId.end());

        // 1. LOCALIZARE PĂRINTE (O singură dată, la început)
        xControl* parentCtrl = LocateAnyControl(parentId);
        if (!parentCtrl) {
            parentCtrl = g_XuiGui.appInstance->getWindow(parentId);
        }
        
        // Validare părinte
        if (!parentCtrl) {
            LOG_ERROR(L"[XUI_DEBUG] Parintele '" + wParentId + L"' nu a fost gasit!");
            return vData{ 0LL };
        }

        // 2. MANAGEMENTUL MENIULUI
        // 1. Managementul Meniului (E special, nu are coordonate)
        if (wType == L"MENU") {
            LOG_DEBUG(L"[XUI_DEBUG] Initializare inteligenta xMenu: " + wId);
            auto menuCtrl = std::make_unique<xMenu>(id, g_XuiGui.appInstance->getEventDispatcher());
            
            xWindow* win = dynamic_cast<xWindow*>(parentCtrl);
            if (win) {
                GtkWidget* winBox = win->getLayoutWidget(); 
                GtkWidget* menuBar = gtk_menu_bar_new();
                
                if (GTK_IS_BOX(winBox)) {
                    // Caz A: Fereastra folosește un aliniament de tip Box
                    gtk_box_pack_start(GTK_BOX(winBox), menuBar, FALSE, FALSE, 0);
                    gtk_box_reorder_child(GTK_BOX(winBox), menuBar, 0); // Îl forțăm să fie primul sus
                    menuCtrl->create(menuBar);
                    gtk_widget_show_all(menuBar); // 🔥 CRITIC: Forțează desenarea elementelor interne
                } 
                else if (GTK_IS_FIXED(winBox)) {
                    // Caz B: Fereastra folosește aliniament absolut (GtkFixed)
                    // Îl punem la coordonatele absolute (0,0) și îi dăm o dimensiune standard de bară
                    gtk_fixed_put(GTK_FIXED(winBox), menuBar, 0, 0);
                    gtk_widget_set_size_request(menuBar, 1200, 25); // Lățime mare implicită, înălțime de 25px
                    menuCtrl->create(menuBar);
                    gtk_widget_show_all(menuBar); // 🔥 CRITIC: Face vizibile textele în interiorul GtkFixed
                }
                else {
                    LOG_WARNING(L"[XUI_DEBUG] Layout-ul ferestrei este de tip necunoscut. Incercam atașare simplă.");
                    menuCtrl->create(nullptr);
                }
            } else {
                menuCtrl->create(nullptr); 
            }
            
            xControl* rawPtr = menuCtrl.get();
            g_XuiGui.controlsMap[id] = rawPtr;
            
            static std::vector<std::unique_ptr<xControl>> m_persistentMenus;
            m_persistentMenus.push_back(std::move(menuCtrl));
            
            return vData{ 1LL };
        }

        // 3. CONTROL STANDARD (Button, Panel, etc.)
        int x = (args.size() > 3) ? static_cast<int>(args[3].toInt()) : 0;
        int y = (args.size() > 4) ? static_cast<int>(args[4].toInt()) : 0;
        int w = (args.size() > 5) ? static_cast<int>(args[5].toInt()) : 0;
        int h = (args.size() > 6) ? static_cast<int>(args[6].toInt()) : 0;
        std::wstring wText = (args.size() > 7) ? args[7].toWString() : L"";

        std::unique_ptr<xControl> newCtrl = nullptr;

        if (wType == L"BUTTON") {
            newCtrl = std::make_unique<xButton>(id, wText, x, y, w, h, g_XuiGui.appInstance->getEventDispatcher());
        }
        else if (wType == L"PANEL") {
            newCtrl = std::make_unique<xPanel>(id, x, y, w, h, g_XuiGui.appInstance->getEventDispatcher());
        }
        else if (wType == L"TABCONTROL") {
            newCtrl = std::make_unique<xTabControl>(id, x, y, w, h, g_XuiGui.appInstance->getEventDispatcher());
        }

        if (newCtrl) {
            xControl* rawControlPtr = newCtrl.get();
            GtkWidget* targetGtkParent = nullptr;
            
            xWindow* parentAsWindow = dynamic_cast<xWindow*>(parentCtrl);
            if (parentAsWindow) {
                targetGtkParent = parentAsWindow->getLayoutWidget();
            } else {
                targetGtkParent = parentCtrl->getHandle();
            }

            if (!targetGtkParent) {
                LOG_ERROR(L"[XUI_DEBUG] Nu am găsit un widget părinte valid pentru adăugare!");
                return vData{ 0LL };
            }

            rawControlPtr->create(targetGtkParent);
            parentCtrl->addChild(id, std::move(newCtrl));
            g_XuiGui.controlsMap[id] = rawControlPtr;
            return vData{ 1LL };
        }

        return vData{ 0LL };
    };
    // =================================================================
    // 4. MANAGEMENTUL PROPRIETĂȚILOR
    // =================================================================

    registry[L"UI_SET_PROPERTY"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ 0LL };

        vData self = args[0];
        std::wstring propName = args[1].toWString();
        vData value = args[2];

        if (!self.isMap()) return vData{ 0LL };
        std::wstring wId = (*self.rawMap())[L"id"].toWString();
        std::string id(wId.begin(), wId.end());

        xControl* ctrl = LocateAnyControl(id);
        if (!ctrl) return vData{ 0LL };

        bool success = ctrl->setProperty(propName, value);
        return vData{ success ? 1LL : 0LL };
    };

    registry[L"UI_GET_PROPERTY"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{ std::monostate{} };

        vData self = args[0];
        std::wstring propName = args[1].toWString();

        if (!self.isMap()) return vData{ std::monostate{} };
        std::wstring wId = (*self.rawMap())[L"id"].toWString();
        std::string id(wId.begin(), wId.end());

        xControl* ctrl = LocateAnyControl(id);
        if (!ctrl) return vData{ std::monostate{} };

        return ctrl->getProperty(propName);
    };

    registry[L"UI_CALL_METHOD"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{ 0LL };

        std::wstring wId = args[0].toWString();
        std::string id(wId.begin(), wId.end());
        std::wstring methodName = args[1].toWString();

        xControl* ctrl = LocateAnyControl(id);
        if (!ctrl) return vData{ 0LL };

        std::vector<vData> methodArgs;
        for (size_t i = 2; i < args.size(); ++i) {
            methodArgs.push_back(args[i]);
        }

        bool success = ctrl->callMethod(methodName, methodArgs);
        return vData{ success ? 1LL : 0LL };
    };

    // =================================================================
    // 5. EVENT BINDING
    // =================================================================

    registry[L"UI_BIND_EVENT"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ 0LL };

        std::wstring wId = args[0].toWString();
        std::wstring wEventName = args[1].toWString();
        std::wstring wScriptFunc = args[2].toWString();

        std::string id(wId.begin(), wId.end());
        std::string eventName(wEventName.begin(), wEventName.end());

        if (!g_XuiGui.appInstance) return vData{ 0LL };

        std::string key = id + "_" + eventName;
        g_PersistentScriptCallbacks[key] = wScriptFunc;

        auto olicallback = [key]() {
            if (g_PersistentScriptCallbacks.count(key) && g_LinkedOliEngine) {
                std::wstring funcName = g_PersistentScriptCallbacks[key];
                std::vector<vData> emptyArgs;
                g_LinkedOliEngine->callUserByteCodeFunction(funcName.c_str(), emptyArgs.data(), 0, vData(0LL));
            }
        };

        g_XuiGui.appInstance->getEventDispatcher().registerHandler(eventName, id, olicallback);
        return vData{ 1LL };
    };

    registry[L"UI_BIND_ARG_EVENT"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ 0LL };

        std::wstring wId = args[0].toWString();
        std::wstring wEventName = args[1].toWString();
        std::wstring wScriptFunc = args[2].toWString();

        std::string id(wId.begin(), wId.end());
        std::string eventName(wEventName.begin(), wEventName.end());

        if (!g_XuiGui.appInstance) return vData{ 0LL };

        auto olicallback = [wScriptFunc](const std::string& arg) {
            if (g_LinkedOliEngine) {
                std::vector<vData> callArgs;
                callArgs.push_back(vData(utf8_to_wstr(arg)));
                g_LinkedOliEngine->callUserByteCodeFunction(wScriptFunc.c_str(), callArgs.data(), (int)callArgs.size(), vData(0LL));
            }
        };

        g_XuiGui.appInstance->getEventDispatcher().registerHandler(eventName, id, olicallback);
        return vData{ 1LL };
    };

    // =================================================================
    // 6. DIALOGURI MODALE NATIVE GTK
    // =================================================================

    registry[L"SHOW_MESSAGE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{ std::monostate{} };

        std::string title = wstr_to_utf8(args[0].toWString());
        std::string message = wstr_to_utf8(args[1].toWString());

        GtkWindow* parentWin = nullptr;
        xControl* mainWin = LocateAnyControl("fereastra_principala");
        if (mainWin && mainWin->getHandle()) {
            parentWin = GTK_WINDOW(mainWin->getHandle());
        }

        GtkWidget* dialog = gtk_message_dialog_new(
            parentWin, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", message.c_str()
        );
        gtk_window_set_title(GTK_WINDOW(dialog), title.c_str());

        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        return vData(L"OK");
    };

    registry[L"UI_SHOW_QUESTION"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData(L"CANCEL");

        std::string title = wstr_to_utf8(args[0].toWString());
        std::string message = wstr_to_utf8(args[1].toWString());

        GtkWindow* parentWin = nullptr;
        xControl* mainWin = LocateAnyControl("fereastra_principala");
        if (mainWin && mainWin->getHandle()) {
            parentWin = GTK_WINDOW(mainWin->getHandle());
        }

        GtkWidget* dialog = gtk_message_dialog_new(
            parentWin, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s", message.c_str()
        );
        gtk_window_set_title(GTK_WINDOW(dialog), title.c_str());

        int result = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        if (result == GTK_RESPONSE_YES) return vData(L"YES");
        if (result == GTK_RESPONSE_NO)  return vData(L"NO");
        return vData(L"CANCEL");
    };

    registry[L"UI_FILE_DIALOG"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData(L"");
        
        int type = (int)args[0].toDouble(); 
        std::string title = wstr_to_utf8(args[1].toWString());

        GtkWindow* parentWin = nullptr;
        xControl* mainWin = LocateAnyControl("fereastra_principala");
        if (mainWin && mainWin->getHandle()) {
            parentWin = GTK_WINDOW(mainWin->getHandle());
        }

        GtkFileChooserAction action = (type == 0) ? GTK_FILE_CHOOSER_ACTION_OPEN : GTK_FILE_CHOOSER_ACTION_SAVE;
        const char* acceptBtn = (type == 0) ? "_Open" : "_Save";

        GtkWidget* dialog = gtk_file_chooser_dialog_new(
            title.c_str(), parentWin, action,
            "_Cancel", GTK_RESPONSE_CANCEL,
            acceptBtn, GTK_RESPONSE_ACCEPT,
            NULL
        );

        if (args.size() > 2) {
            std::string initialPath = wstr_to_utf8(args[2].toWString());
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), initialPath.c_str());
        }

        std::wstring selectedPath = L"";
        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            selectedPath = utf8_to_wstr(filename);
            g_free(filename);
        }

        gtk_widget_destroy(dialog);
        return vData(selectedPath);
    };
}

OLI_EXPORT void SetPluginConsoleManager(ConsoleManager* hostCm) {
    if (hostCm != nullptr) {
        ConsoleManager::setInstance(hostCm);
    }
}