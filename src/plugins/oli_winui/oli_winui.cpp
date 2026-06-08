#include "../../OliEngine.hpp"
#include "../../IOliEngine.hpp"
#include "Layouts/Layouts.hpp"
#include "vApp.hpp"
#include "vWindow.hpp"
#include "vPanel.hpp"
#include "vButton.hpp"
#include "vFileDialog.hpp"
#include "vCodeView.hpp"
#include "vMenu.hpp"
#include "vMessageDialog.hpp"
#include "vTabControl.hpp"
#include "vRichConsole.hpp"
#include <unordered_map>
#include <memory>
#include <sstream>
#include <map>

#ifdef _WIN32
#define OLI_EXPORT extern "C" __declspec(dllexport)
#else
#define OLI_EXPORT extern "C"
#endif


// Variabile temporare pentru controlul poziționării ferestrei de dialog
static HHOOK g_hCbtHook = nullptr;
static HWND g_hTargetParent = nullptr;

// Funcția callback apelată de Windows chiar înainte ca MessageBox-ul să devină vizibil
static LRESULT CALLBACK MessageBoxCbtProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HCBT_ACTIVATE) {
        HWND hwndMsgBox = reinterpret_cast<HWND>(wParam);

        if (g_hTargetParent && IsWindow(g_hTargetParent)) {
            RECT rcOwner, rcMsg;
            GetWindowRect(g_hTargetParent, &rcOwner); // Coordonatele IDE-ului
            GetWindowRect(hwndMsgBox, &rcMsg);       // Coordonatele Message Box-ului

            int ownerWidth = rcOwner.right - rcOwner.left;
            int ownerHeight = rcOwner.bottom - rcOwner.top;
            int msgWidth = rcMsg.right - rcMsg.left;
            int msgHeight = rcMsg.bottom - rcMsg.top;

            // Calculăm centrul perfect raportat la fereastra principală
            int x = rcOwner.left + (ownerWidth - msgWidth) / 2;
            int y = rcOwner.top + (ownerHeight - msgHeight) / 2;

            // Mutăm dialogul pe noile coordonate centrate
            SetWindowPos(hwndMsgBox, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }

        // Dezactivăm hook-ul imediat ce ne-am terminat treaba
        UnhookWindowsHookEx(g_hCbtHook);
        g_hCbtHook = nullptr;
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}


using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

struct GuiState {
    std::unique_ptr<vApp> ownedAppInstance = nullptr;
    vApp* appInstance = nullptr;
    bool isInitialized = false;
    std::unordered_map<std::string, vControl*> controlsMap;
} g_Gui;

// Pointerul global legitim către mașina virtuală din EXE
inline vOliEngine* g_LinkedOliEngine = nullptr;

// Memoria persistentă a callback-urilor din script
static std::map<std::string, std::wstring> g_PersistentScriptCallbacks;

vControl* LocateAnyControl(const std::string& id) {
    if (!g_Gui.appInstance) return nullptr;
    auto it = g_Gui.controlsMap.find(id);
    if (it != g_Gui.controlsMap.end()) {
        return it->second;
    }
    return nullptr;
}

// ✅ PUNCTUL UNIC DE INTRARE: Compilatorul și Linkerul vor expune doar această semnătură curată
OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry, void* enginePtr) {

    if (g_LinkedOliEngine == nullptr) {
        g_LinkedOliEngine = static_cast<vOliEngine*>(enginePtr);

        if (g_LinkedOliEngine) {
            LOG_SUCCESS(L"✅ [DLL] Conexiune stabilă stabilită cu OliEngine-ul Gazdă!");
        }
        else {
            LOG_ERROR(L"❌ [DLL] Eroare critică: Am primit un pointer enginePtr de tip NULL!");
        }
    }
    else {
        LOG_INFO(L"ℹ️ [DLL] LoadOliPlugin re-apelat (Compilare/Context secundar). Păstrăm engine-ul gazdă intact.");
    }

    // ==========================================
    // 1. COMPONENTA: WINAPP (Aplicația principală)
    // ==========================================

    registry[L"CREATE_WINAPP"] = [](const std::vector<vData>&) -> vData {
        auto objMap = std::make_shared<std::unordered_map<std::wstring, vData>>();
        (*objMap)[L"__type__"] = vData(L"WinApp");
        return vData(objMap);
        };

    registry[L"WINAPP::INIT"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData{ 0LL };
        if (g_Gui.isInitialized) return vData{ 1LL };

        g_Gui.appInstance = vApp::getAppInstance();

        if (!g_Gui.appInstance) {
            HINSTANCE hInst = GetModuleHandle(nullptr);
            g_Gui.ownedAppInstance = std::make_unique<vApp>(hInst, RunMode::GUI);
            g_Gui.appInstance = g_Gui.ownedAppInstance.get();

            if (!g_Gui.appInstance->init()) {
                g_Gui.ownedAppInstance.reset();
                g_Gui.appInstance = nullptr;
                return vData{ 0LL };
            }
        }

        g_Gui.isInitialized = true;
        return vData{ 1LL };
        };

    registry[L"WINAPP::RUN"] = [](const std::vector<vData>&) -> vData {
        if (!g_Gui.isInitialized || !g_Gui.appInstance) {
            return vData{ 0LL };
        }
        int exitCode = g_Gui.appInstance->run();
        return vData{ static_cast<long long>(exitCode) };
        };


    // ==========================================
    // 2. COMPONENTA: WINWINDOW (Management ferestre)
    // ==========================================

    registry[L"CREATE_WINWINDOW"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{ std::monostate{} };

        try {
            std::wstring wId = args[0].toWString();
            std::wstring wTitle = args[1].toWString();
            std::string id(wId.begin(), wId.end());

            LOG_DEBUG(L"[DLL_DEBUG] Titlu primit: " + wTitle + L" | Lungime: " + std::to_wstring(wTitle.length()));

            auto objMap = std::make_shared<std::unordered_map<std::wstring, vData>>();
            (*objMap)[L"__type__"] = vData(L"WinWindow");
            (*objMap)[L"id"] = vData(wId);
            (*objMap)[L"title"] = vData(wTitle);

            HINSTANCE hInst = GetModuleHandle(nullptr);

            vApp* actualApp = vApp::getAppInstance();
            if (!actualApp) {
                LOG_ERROR(L"❌ [CREATE_WINWINDOW] appInstance este NULL în contextul curent!");
                return vData{ std::monostate{} };
            }

            LOG_DEBUG(L"[DLL_DEBUG] Instantiem vWindow folosind contextul verificat al executabilului...");

            auto newWindow = std::make_unique<vWindow>(
                hInst, id, WindowType::StandardWindow, false,
                actualApp->getEventDispatcher()
            );
            newWindow->setIsMainWindow(true);

            bool created = newWindow->create(L"VOliWindowClass", wTitle,
                WS_OVERLAPPEDWINDOW, 200, 200, 800, 900, nullptr, nullptr);

            if (created) {
                vWindow* winPtr = newWindow.get();
                actualApp->addWindow(id, std::move(newWindow));
                g_Gui.controlsMap[id] = winPtr;
                LOG_DEBUG(L"[DLL_DEBUG] Fereastra a fost creata si inregistrata in siguranta.");
            }
            else {
                LOG_ERROR(L"❌ [CREATE_WINWINDOW] CreateWindowExW a returnat NULL!");
            }

            return vData(objMap);
        }
        catch (...) {
            LOG_ERROR(L"❌ [CREATE_WINWINDOW] Crash hardware / acces ilegal interceptat definitiv!");
            return vData{ std::monostate{} };
        }
        };

    registry[L"WINWINDOW::SHOW"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData{ 0LL };
        vData self = args[0];

        if (!self.isMap()) return vData{ 0LL };
        auto m = self.rawMap();

        std::wstring wId = (*m)[L"id"].toWString();
        std::string id(wId.begin(), wId.end());

        vControl* ctrl = LocateAnyControl(id);
        if (ctrl) {
            ctrl->show();
            return vData{ 1LL };
        }
        return vData{ 0LL };
        };


    // =================================================================
    // 3. CONSTRUCTORUL GENERIC DE CONTROALE
    // =================================================================

    registry[L"UI_CREATE_CONTROL"] = [](const std::vector<vData>& args) -> vData {
        LOG_DEBUG(L"[DLL_DEBUG] Intrare UI_CREATE_CONTROL. Numar argumente primite: " + std::to_wstring(args.size()));

        if (args.size() < 3) {
            LOG_ERROR(L"[DLL_DEBUG] Respingeri apel: args.size() mai mic de 3!");
            return vData{ 0LL };
        }

        std::wstring wType = args[0].toWString();
        std::wstring wId = args[1].toWString();
        std::wstring wParentId = args[2].toWString();

        std::string id(wId.begin(), wId.end());
        std::string parentId(wParentId.begin(), wParentId.end());

        LOG_DEBUG(L"[DLL_DEBUG] Procesare control tip: '" + wType + L"', ID: '" + wId + L"', Parinte: '" + wParentId + L"'");

        if (!g_Gui.isInitialized || !g_Gui.appInstance) {
            LOG_ERROR(L"[DLL_DEBUG] Eroare: GUI nu este initializat sau appInstance este NULL!");
            return vData{ 0LL };
        }

        vControl* parentCtrl = LocateAnyControl(parentId);
        if (parentCtrl) {
            LOG_DEBUG(L"[DLL_DEBUG] Parintele '" + wParentId + L"' gasit cu succes in controlsMap. Adresa: " + std::to_wstring(reinterpret_cast<uintptr_t>(parentCtrl)));
        }
        else {
            LOG_DEBUG(L"[DLL_DEBUG] Parintele '" + wParentId + L"' NU e in controlsMap. Incercam prin getWindow...");
            parentCtrl = g_Gui.appInstance->getWindow(parentId);

            if (parentCtrl) {
                LOG_DEBUG(L"[DLL_DEBUG] Parintele '" + wParentId + L"' a fost gasit ca Window legitim. Adresa: " + std::to_wstring(reinterpret_cast<uintptr_t>(parentCtrl)));
            }
            else {
                LOG_ERROR(L"[DLL_DEBUG] CRITIC: Parintele '" + wParentId + L"' nu a putut fi localizat nicaieri!");
                return vData{ 0LL };
            }
        }

        HINSTANCE hInst = g_Gui.appInstance->getInstance();

        if (wType == L"MENU") {
            try {
                LOG_DEBUG(L"[DLL_DEBUG] Initializare vMenu: " + wId);

                auto menuCtrl = std::make_shared<vMenu>(id, g_Gui.appInstance->getEventDispatcher());
                menuCtrl->create(nullptr);

                vControl* rawPtr = menuCtrl.get();
                LOG_DEBUG(L"[DLL_DEBUG] vMenu creat. Adresa pointer: " + std::to_wstring(reinterpret_cast<uintptr_t>(rawPtr)));

                g_Gui.controlsMap[id] = rawPtr;

                static std::vector<std::shared_ptr<vControl>> m_persistentMenus;
                m_persistentMenus.push_back(menuCtrl);

                LOG_DEBUG(L"[DLL_DEBUG] vMenu '" + wId + L"' inregistrat cu succes (fără addChild).");
                return vData{ 1LL };
            }
            catch (const std::exception& e) {
                LOG_ERROR(L"[DLL_DEBUG] Exceptie menu: " + std::wstring(e.what(), e.what() + strlen(e.what())));
                return vData{ 0LL };
            }
        }

        try {
            LOG_DEBUG(L"[DLL_DEBUG] Incepe parsarea coordonatelor optionale pentru control standard...");

            int x = (args.size() > 3) ? static_cast<int>(args[3].toInt()) : 0;
            int y = (args.size() > 4) ? static_cast<int>(args[4].toInt()) : 0;
            int w = (args.size() > 5) ? static_cast<int>(args[5].toInt()) : 0;
            int h = (args.size() > 6) ? static_cast<int>(args[6].toInt()) : 0;
            std::wstring wText = (args.size() > 7) ? args[7].toWString() : L"";

            LOG_DEBUG(L"[DLL_DEBUG] Coordonate parsat: x=" + std::to_wstring(x) + L", y=" + std::to_wstring(y) + L", w=" + std::to_wstring(w) + L", h=" + std::to_wstring(h));

            std::unique_ptr<vControl> newCtrl = nullptr;

            if (wType == L"BUTTON") {
                newCtrl = std::make_unique<vButton>(
                    hInst, id, wText, x, y, w, h, g_Gui.appInstance->getEventDispatcher()
                );
            }
            else if (wType == L"PANEL") {
                newCtrl = std::make_unique<vPanel>(
                    hInst, id, x, y, w, h, g_Gui.appInstance->getEventDispatcher()
                );
            }
            else if (wType == L"TABCONTROL") {
                newCtrl = std::make_unique<vTabControl>(
                    hInst, id, x, y, w, h, g_Gui.appInstance->getEventDispatcher()
                );
            }
            else if (wType == L"CODEVIEW") {
                newCtrl = std::make_unique<vCodeView>(
                    hInst, id, x, y, w, h, g_Gui.appInstance->getEventDispatcher()
                );
            }
            else if (wType == L"RICHEDIT") {
                newCtrl = std::make_unique<vRichEdit>(
                    hInst, id, x, y, w, h, g_Gui.appInstance->getEventDispatcher()
                );
            }
            else if (wType == L"RICHCONSOLE") {
                newCtrl = std::make_unique<vRichConsole>(
                    hInst, id, x, y, w, h, g_Gui.appInstance->getEventDispatcher()
                );
            }
            else {
                LOG_ERROR(L"[DLL_DEBUG] Tip de control necunoscut/nesuportat: '" + wType + L"'");
            }

            if (newCtrl) {
                vControl* rawControlPtr = newCtrl.get();

                LOG_DEBUG(L"[DLL_DEBUG] Adaugam controlul in ierarhia parintelui...");
                parentCtrl->addChild(id, std::move(newCtrl));

                LOG_DEBUG(L"[DLL_DEBUG] Mapam controlul in controlsMap...");
                g_Gui.controlsMap[id] = rawControlPtr;

                LOG_DEBUG(L"[DLL_DEBUG] Control '" + wId + L"' creat cu succes.");
                return vData{ 1LL };
            }
        }
        catch (const std::exception& e) {
            std::wstring errMsg(e.what(), e.what() + strlen(e.what()));
            LOG_ERROR(L"[DLL_DEBUG] EXCEPTIE CRITICA PRINSA LA CONTROL STANDARD: " + errMsg);
            return vData{ 0LL };
        }
        catch (...) {
            LOG_ERROR(L"[DLL_DEBUG] CRASH ANONIM PRINS in blocul standard!");
            return vData{ 0LL };
        }

        LOG_ERROR(L"[DLL_DEBUG] Iesire din functie cu fail (newCtrl a ramas null).");
        return vData{ 0LL };
        };


    registry[L"UI_SET_PROPERTY"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ 0LL };

        vData self = args[0];
        std::wstring propName = args[1].toWString();
        vData value = args[2];

        if (!self.isMap()) return vData{ 0LL };
        std::wstring wId = (*self.rawMap())[L"id"].toWString();
        std::string id(wId.begin(), wId.end());

        vControl* ctrl = LocateAnyControl(id);
        if (!ctrl) return vData{ 0LL };

        if (propName == L"menu") {
            std::wstring wMenuId = value.toWString();
            std::string menuId(wMenuId.begin(), wMenuId.end());

            vControl* menuCtrl = LocateAnyControl(menuId);
            vMenu* pMenu = dynamic_cast<vMenu*>(menuCtrl);
            vWindow* pWin = dynamic_cast<vWindow*>(ctrl);

            if (pWin && pMenu) {
                pWin->setMenu(pMenu);
                return vData{ 1LL };
            }
            return vData{ 0LL };
        }

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

        vControl* ctrl = LocateAnyControl(id);
        if (!ctrl) return vData{ std::monostate{} };

        return ctrl->getProperty(propName);
        };

    // =================================================================
    // 6. LEGAREA EVENIMENTELOR: UI_BIND_EVENT
    // =================================================================

    registry[L"UI_BIND_EVENT"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ 0LL };

        std::wstring wId = args[0].toWString();
        std::wstring wEventName = args[1].toWString();
        std::wstring wScriptFunc = args[2].toWString();

        std::string id(wId.begin(), wId.end());
        std::string eventName(wEventName.begin(), wEventName.end());

        if (!g_Gui.appInstance) return vData{ 0LL };

        std::string key = id + "_" + eventName;
        g_PersistentScriptCallbacks[key] = wScriptFunc;

        auto olicallback = [key]() {
            if (g_PersistentScriptCallbacks.count(key)) {
                std::wstring funcName = g_PersistentScriptCallbacks[key];

                if (g_LinkedOliEngine) {
                    std::vector<vData> emptyArgs;
                    g_LinkedOliEngine->callUserByteCodeFunction(
                        funcName.c_str(),
                        emptyArgs.data(),
                        0,
                        vData(0LL)
                    );
                }
                else {
                    LOG_ERROR(L"❌ [CRITIC] g_LinkedOliEngine este NULL la declanșarea evenimentului!");
                }
            }
            };

        g_Gui.appInstance->getEventDispatcher().registerHandler(eventName, id, olicallback);
        return vData{ 1LL };
        };


    registry[L"UI_BIND_ARG_EVENT"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ 0LL };

        std::wstring wId = args[0].toWString();
        std::wstring wEventName = args[1].toWString();
        std::wstring wScriptFunc = args[2].toWString();

        std::string id(wId.begin(), wId.end());
        std::string eventName(wEventName.begin(), wEventName.end());

        if (!g_Gui.appInstance) return vData{ 0LL };

        auto olicallback = [wScriptFunc](const std::string& arg) {
            if (g_LinkedOliEngine) {
                std::vector<vData> callArgs;
                callArgs.push_back(vData(std::wstring(arg.begin(), arg.end())));

                g_LinkedOliEngine->callUserByteCodeFunction(
                    wScriptFunc.c_str(),
                    callArgs.data(),
                    (int)callArgs.size(),
                    vData(0LL)
                );
            }
            };

        g_Gui.appInstance->getEventDispatcher().registerHandler(eventName, id, olicallback);
        return vData{ 1LL };
        };

    registry[L"UI_CALL_METHOD"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{ 0LL };

        std::wstring wId = args[0].toWString();
        std::string id(wId.begin(), wId.end());
        std::wstring methodName = args[1].toWString();

        LOG_DEBUG(L"[DLL_DEBUG] DEBUG: methodName primit este: '" + methodName + L"'");

        vControl* ctrl = LocateAnyControl(id);
        if (!ctrl) return vData{ 0LL };

        std::vector<vData> methodArgs;
        for (size_t i = 2; i < args.size(); ++i) {
            methodArgs.push_back(args[i]);
        }

        bool success = ctrl->callMethod(methodName, methodArgs);
        return vData{ success ? 1LL : 0LL };
        };

    registry[L"SHOW_MESSAGE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{ std::monostate{} };

        std::wstring title = args[0].toWString();
        std::wstring message = args[1].toWString();

        std::string result = vMessageDialog::show(title, message, MessageButtons::Ok);

        return vData(std::wstring(result.begin(), result.end()));
        };

    // =================================================================
    // 🔥 NOU: CONEXIUNE DIALOG CONFIRMARE (YES / NO / CANCEL) PENTRU SCRIPT
    // =================================================================
    registry[L"UI_SHOW_QUESTION"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData(L"CANCEL");

        std::wstring title = args[0].toWString();
        std::wstring message = args[1].toWString();

        // 1. Încercăm să extragem handle-ul nativ (HWND) al ferestrei tale principale
        HWND hParent = GetActiveWindow();
        vControl* mainWin = LocateAnyControl("fereastra_principala");
        if (mainWin && mainWin->getHandle()) {
            hParent = mainWin->getHandle();
        }

        // 2. Configurăm variabilele globale pentru hook și injectăm Hook-ul pe firul UI curent
        g_hTargetParent = hParent;
        g_hCbtHook = SetWindowsHookEx(WH_CBT, MessageBoxCbtProc, nullptr, GetCurrentThreadId());

        // 3. Lansăm caseta nativă modală (se va centra perfect peste IDE)
        int result = MessageBoxW(hParent, message.c_str(), title.c_str(), MB_YESNOCANCEL | MB_ICONQUESTION);

        // Plasă de siguranță: dacă din orice motiv ciudat hook-ul a rămas activ, îl distrugem
        if (g_hCbtHook) {
            UnhookWindowsHookEx(g_hCbtHook);
            g_hCbtHook = nullptr;
        }

        if (result == IDYES) return vData(L"YES");
        if (result == IDNO)  return vData(L"NO");
        return vData(L"CANCEL");
        };

    registry[L"UI_FILE_DIALOG"] = [](const std::vector<vData>& args) -> vData {
        int type = (int)args[0].toDouble();
        std::wstring title = args[1].toWString();

        WinFileDialog dlg(title);

        if (args.size() > 2) {
            dlg.setInitialPath(args[2].toWString());
        }

        if (type == 0) { // OPEN
            if (dlg.showOpen(nullptr)) return vData(dlg.getFilePath());
        }
        else { // SAVE
            if (dlg.showSave(nullptr)) return vData(dlg.getFilePath());
        }

        return vData(L"");
        };
}

OLI_EXPORT void SetPluginConsoleManager(ConsoleManager* hostCm) {
    if (hostCm != nullptr) {
        ConsoleManager::setInstance(hostCm);
    }
}