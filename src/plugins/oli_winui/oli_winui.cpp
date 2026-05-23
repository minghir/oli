#include "../../OliEngine.hpp"
#include "../../IOliEngine.hpp"
#include "vApp.hpp"
#include "vWindow.hpp"
#include "vPanel.hpp"
#include "vButton.hpp"
#include "vCodeView.hpp"
#include <unordered_map>
#include <memory>
#include <sstream>
#include <map>

#ifdef _WIN32
#define OLI_EXPORT extern "C" __declspec(dllexport)
#else
#define OLI_EXPORT extern "C"
#endif

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

struct GuiState {
    std::unique_ptr<vApp> ownedAppInstance = nullptr;
    vApp* appInstance = nullptr;
    bool isInitialized = false;
    std::unordered_map<std::string, vControl*> controlsMap;
} g_Gui;

// Pointerul global legitim către mașina virtuală din EXE
static vOliEngine* g_LinkedOliEngine = nullptr;

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

    // Salvăm și castăm corect pointerul motorului primit direct din internalLoadPlugin-ul EXE-ului
    g_LinkedOliEngine = static_cast<vOliEngine*>(enginePtr);

    if (g_LinkedOliEngine) {
        LOG_SUCCESS(L"✅ [DLL] Conexiune stabilă cu OliEngine! Pointer salvat.");
    }
    else {
        LOG_ERROR(L"❌ [DLL] Eroare critică: Am primit un pointer enginePtr de tip NULL!");
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

        std::wstring wId = std::get<std::wstring>(args[0].value);
        std::wstring wTitle = std::get<std::wstring>(args[1].value);

        auto objMap = std::make_shared<std::unordered_map<std::wstring, vData>>();
        (*objMap)[L"__type__"] = vData(L"WinWindow");
        (*objMap)[L"id"] = vData(wId);
        (*objMap)[L"title"] = vData(wTitle);

        if (g_Gui.isInitialized && g_Gui.appInstance) {
            std::string id(wId.begin(), wId.end());
            HINSTANCE hInst = g_Gui.appInstance->getInstance();

            auto newWindow = std::make_unique<vWindow>(
                hInst, id, WindowType::StandardWindow, false,
                g_Gui.appInstance->getEventDispatcher()
            );
            newWindow->setIsMainWindow(true);

            bool created = newWindow->create(L"VOliWindowClass", wTitle,
                WS_OVERLAPPEDWINDOW, 200, 200, 800, 600, nullptr, nullptr);

            if (created) {
                vWindow* winPtr = newWindow.get();
                g_Gui.appInstance->addWindow(id, std::move(newWindow));
                g_Gui.controlsMap[id] = winPtr;
            }
        }

        return vData(objMap);
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
        if (args.size() < 3) return vData{ 0LL };

        std::wstring wType = args[0].toWString();
        std::wstring wId = args[1].toWString();
        std::wstring wParentId = args[2].toWString();

        std::string id(wId.begin(), wId.end());
        std::string parentId(wParentId.begin(), wParentId.end());

        if (!g_Gui.isInitialized || !g_Gui.appInstance) return vData{ 0LL };

        vControl* parentCtrl = LocateAnyControl(parentId);
        if (!parentCtrl) return vData{ 0LL };

        HINSTANCE hInst = g_Gui.appInstance->getInstance();
        std::unique_ptr<vControl> newCtrl = nullptr;

        int x = (args.size() > 3) ? static_cast<int>(args[3].toInt()) : 0;
        int y = (args.size() > 4) ? static_cast<int>(args[4].toInt()) : 0;
        int w = (args.size() > 5) ? static_cast<int>(args[5].toInt()) : 0;
        int h = (args.size() > 6) ? static_cast<int>(args[6].toInt()) : 0;
        std::wstring wText = (args.size() > 7) ? args[7].toWString() : L"";

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
        else if (wType == L"CODEVIEW") {
            newCtrl = std::make_unique<vCodeView>(
                hInst, id, x, y, w, h, g_Gui.appInstance->getEventDispatcher()
            );
        }

        if (newCtrl) {
            vControl* rawControlPtr = newCtrl.get();
            parentCtrl->addChild(id, std::move(newCtrl));
            g_Gui.controlsMap[id] = rawControlPtr;
            return vData{ 1LL };
        }

        return vData{ 0LL };
        };

    // =================================================================
    // 4. DISPECERUL UNIVERSAL DE SETĂRI
    // =================================================================

    registry[L"UI_SET_PROPERTY"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ 0LL };

        vData self = args[0];
        std::wstring propName = to_lower(args[1].toWString());
        vData value = args[2];

        if (!self.isMap()) return vData{ 0LL };
        std::wstring wId = (*self.rawMap())[L"id"].toWString();
        std::string id(wId.begin(), wId.end());

        vControl* ctrl = LocateAnyControl(id);
        if (!ctrl) return vData{ 0LL };

        if (propName == L"text") {
            ctrl->setText(value.toWString());
        }
        else if (propName == L"x") {
            ctrl->setX(static_cast<int>(value.toInt()));
            MoveWindow(ctrl->getHandle(), ctrl->getX(), ctrl->getY(), ctrl->getWidth(), ctrl->getHeight(), TRUE);
        }
        else if (propName == L"y") {
            ctrl->setY(static_cast<int>(value.toInt()));
            MoveWindow(ctrl->getHandle(), ctrl->getX(), ctrl->getY(), ctrl->getWidth(), ctrl->getHeight(), TRUE);
        }
        else if (propName == L"width") {
            ctrl->setWidth(static_cast<int>(value.toInt()));
            MoveWindow(ctrl->getHandle(), ctrl->getX(), ctrl->getY(), ctrl->getWidth(), ctrl->getHeight(), TRUE);
        }
        else if (propName == L"height") {
            ctrl->setHeight(static_cast<int>(value.toInt()));
            MoveWindow(ctrl->getHandle(), ctrl->getX(), ctrl->getY(), ctrl->getWidth(), ctrl->getHeight(), TRUE);
        }
        else if (propName == L"visible") {
            if (value.toBool()) {
                ShowWindow(ctrl->getHandle(), SW_SHOW);
                ctrl->show();
            }
            else {
                ctrl->hide();
            }
        }

        return vData{ 1LL };
        };

    // =================================================================
    // 5. DISPECERUL UNIVERSAL DE CITIRI
    // =================================================================

    registry[L"UI_GET_PROPERTY"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{ std::monostate{} };

        vData self = args[0];
        std::wstring propName = to_lower(args[1].toWString());

        if (!self.isMap()) return vData{ std::monostate{} };
        std::wstring wId = (*self.rawMap())[L"id"].toWString();

        if (propName == L"id") {
            return vData(wId);
        }

        std::string id(wId.begin(), wId.end());
        vControl* ctrl = LocateAnyControl(id);
        if (!ctrl) return vData{ std::monostate{} };

        if (propName == L"text")       return vData(ctrl->getText());
        if (propName == L"x")          return vData(static_cast<long long>(ctrl->getX()));
        if (propName == L"y")          return vData(static_cast<long long>(ctrl->getY()));
        if (propName == L"width")      return vData(static_cast<long long>(ctrl->getWidth()));
        if (propName == L"height")     return vData(static_cast<long long>(ctrl->getHeight()));
        if (propName == L"enabled")    return vData(ctrl->isEnabled());
        if (propName == L"visible")    return vData(ctrl->isVisible());
        if (propName == L"font_size")  return vData(static_cast<long long>(ctrl->getFontSize()));
        if (propName == L"font_name")  return vData(ctrl->getFontName());

        if (ctrl->hasAttribute(args[1].toWString())) {
            return vData(ctrl->getAttribute(args[1].toWString()));
        }

        return vData{ std::monostate{} };
        };

    // =================================================================
    // 6. LEGAREA EVENIMENTELOR: UI_BIND_EVENT (Fără verificarea g_Gui.oliEngine)
    // =================================================================

    registry[L"UI_BIND_EVENT"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ 0LL };

        std::wstring wId = args[0].toWString();
        std::wstring wEventName = args[1].toWString();
        std::wstring wScriptFunc = args[2].toWString();

        std::string id(wId.begin(), wId.end());
        std::string eventName(wEventName.begin(), wEventName.end());

        // Eliminat g_Gui.oliEngine fugar! Ne bazăm exclusiv pe g_Gui.appInstance
        if (!g_Gui.appInstance) return vData{ 0LL };

        std::string key = id + "_" + eventName;
        g_PersistentScriptCallbacks[key] = wScriptFunc;

        auto olicallback = [key]() {
            if (g_PersistentScriptCallbacks.count(key)) {
                std::wstring funcName = g_PersistentScriptCallbacks[key];

                // Invocăm prin intermediul pointerului preluat de la intrare
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
}