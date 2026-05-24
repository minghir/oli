#include "../../OliEngine.hpp"
#include "../../IOliEngine.hpp"
#include "Layouts/Layouts.hpp"
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
            // Încercăm să vedem dacă este un vCodeView
            vCodeView* codeView = dynamic_cast<vCodeView*>(ctrl);
            if (codeView) {
                // Dacă este vCodeView, apelăm funcția lui specifică ce conține și highlight-ul
                codeView->setText(value.toWString());
            }
            else {
                // Altfel, apelăm comportamentul standard pentru restul controalelor (butoane, label-uri etc.)
                ctrl->setText(value.toWString());
            }
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
        else if (propName == L"layout") {
            // 1. Încercăm să convertim controlul la tipul vContainer
            vContainer* container = dynamic_cast<vContainer*>(ctrl);

            if (!container) {
                LOG_ERROR(L"❌ Proprietatea 'layout' poate fi setată doar pe obiecte de tip Container (Window, Panel etc.)! ID: " + args[0].toWString());
                return vData{ 0LL };
            }

            std::wstring layoutType = to_upper(value.toWString());

            // 2. Instanțiem strategia în funcție de ce a cerut scriptul Oli
            if (layoutType == L"GRID") {
                // Dacă constructorul de Grid cere parametri (ex: randuri, coloane), îi poți citi din argumente suplimentare sau pune default
                container->setLayoutStrategy(std::make_unique<GridLayout>(1,1));
                LOG_SUCCESS(L"📐 [Layout] Strategia GRID a fost aplicată pe containerul: " + wId);
            }
            else if (layoutType == L"ANCHOR") {
                // Dacă constructorul de Grid cere parametri (ex: randuri, coloane), îi poți citi din argumente suplimentare sau pune default
                container->setLayoutStrategy(std::make_unique<AnchorLayout>());
                LOG_SUCCESS(L"📐 [Layout] Strategia ANCHOR a fost aplicată pe containerul: " + wId);
            }
            else if (layoutType == L"FLOW") {
                container->setLayoutStrategy(std::make_unique<FormLayout>());
                LOG_SUCCESS(L"📐 [Layout] Strategia FLOW a fost aplicată pe containerul: " + wId);
            }
            else if (layoutType == L"VSTACK") {
                container->setLayoutStrategy(std::make_unique<VerticalStackLayout>());
                LOG_SUCCESS(L"📐 [Layout] Strategia VSTACK a fost aplicată pe containerul: " + wId);
            }
            else if (layoutType == L"HSTACK") {
                container->setLayoutStrategy(std::make_unique<HorizontalPercentStackLayout>());
                LOG_SUCCESS(L"📐 [Layout] Strategia HSTACK a fost aplicată pe containerul: " + wId);
            }
            else if (layoutType == L"NONE" || layoutType == L"NULL") {
                container->setLayoutStrategy(nullptr);
                LOG_INFO(L"📐 [Layout] Strategia de layout a fost eliminată pentru: " + wId);
            }
            //HorizontalPercentStackLayout
            else {
                LOG_ERROR(L"❌ Strategie de layout necunoscută: " + layoutType);
                return vData{ 0LL };
            }
        }
        // ==========================================
    // EXTENSIE LAYOUT & EMULARE SIZEMODE / ANCHOR
    // ==========================================
        else if (propName == L"anchor") {
            std::wstring valStr = to_upper(value.toWString());
            Anchor finalAnchor = Anchor::NONE;

            // Suport pentru combinări de flag-uri prin caractere de separare (ex: "LEFT|TOP" sau "LEFT+TOP")
            if (valStr.find(L"LEFT") != std::wstring::npos)   finalAnchor = finalAnchor | Anchor::LEFT;
            if (valStr.find(L"RIGHT") != std::wstring::npos)  finalAnchor = finalAnchor | Anchor::RIGHT;
            if (valStr.find(L"TOP") != std::wstring::npos)    finalAnchor = finalAnchor | Anchor::TOP;
            if (valStr.find(L"BOTTOM") != std::wstring::npos) finalAnchor = finalAnchor | Anchor::BOTTOM;
            if (valStr.find(L"CENTER_H") != std::wstring::npos) finalAnchor = finalAnchor | Anchor::CENTER_H;
            if (valStr.find(L"CENTER_V") != std::wstring::npos) finalAnchor = finalAnchor | Anchor::CENTER_V;

            // Cazul simplu când se scrie direct "CENTER"
            if (valStr == L"CENTER") finalAnchor = Anchor::CENTER;

            ctrl->setAnchor(finalAnchor);

            // Declanșăm recalcularea layout-ului pe părinte, dacă acesta este un container active
            if (ctrl->getParent()) {
                vContainer* parentContainer = dynamic_cast<vContainer*>(ctrl->getParent());
                if (parentContainer) parentContainer->applyLayout();
            }
        }
        else if (propName == L"width_mode") {
            std::wstring valStr = to_upper(value.toWString());

            if (valStr == L"FIXED")        ctrl->setWidthMode(SizeMode::FIXED);
            else if (valStr == L"FILL")    ctrl->setWidthMode(SizeMode::FILL);
            else if (valStr == L"AUTO")    ctrl->setWidthMode(SizeMode::AUTO);
            else if (valStr == L"PERCENT") ctrl->setWidthMode(SizeMode::PERCENT);
            else {
                LOG_ERROR(L"❌ [Layout] SizeMode necunoscut pentru width_mode: " + valStr);
                return vData{ 0LL };
            }

            if (ctrl->getParent()) {
                vContainer* parentContainer = dynamic_cast<vContainer*>(ctrl->getParent());
                if (parentContainer) parentContainer->applyLayout();
            }
        }
        else if (propName == L"height_mode") {
            std::wstring valStr = to_upper(value.toWString());

            if (valStr == L"FIXED")        ctrl->setHeightMode(SizeMode::FIXED);
            else if (valStr == L"FILL")    ctrl->setHeightMode(SizeMode::FILL);
            else if (valStr == L"AUTO")    ctrl->setHeightMode(SizeMode::AUTO);
            else if (valStr == L"PERCENT") ctrl->setHeightMode(SizeMode::PERCENT);
            else {
                LOG_ERROR(L"❌ [Layout] SizeMode necunoscut pentru height_mode: " + valStr);
                return vData{ 0LL };
            }

            if (ctrl->getParent()) {
                vContainer* parentContainer = dynamic_cast<vContainer*>(ctrl->getParent());
                if (parentContainer) parentContainer->applyLayout();
            }
        }
        else if (propName == L"margin") {
            // Suportă sintaxă de tip vector sau listă din script (ex: $margin = [10, 5, 10, 5])
            if (value.isArray()) {
                auto* arr = value.rawArray();
                if (arr && arr->size() >= 4) {
                    ctrl->setMargins(
                        static_cast<int>((*arr)[0].toInt()),
                        static_cast<int>((*arr)[1].toInt()),
                        static_cast<int>((*arr)[2].toInt()),
                        static_cast<int>((*arr)[3].toInt())
                    );
                }
            }
            else {
                // Dacă trimitem un singur număr întreg, îl aplicăm pe toate laturile uniform (ex: 10)
                int uniformMargin = static_cast<int>(value.toInt());
                ctrl->setMargins(uniformMargin, uniformMargin, uniformMargin, uniformMargin);
            }

            if (ctrl->getParent()) {
                vContainer* parentContainer = dynamic_cast<vContainer*>(ctrl->getParent());
                if (parentContainer) parentContainer->applyLayout();
            }
        }
        else if (propName == L"syntax_path") {
            // 1. Încercăm să convertim controlul generic la tipul specific vCodeView
            vCodeView* codeView = dynamic_cast<vCodeView*>(ctrl);

            if (!codeView) {
                LOG_ERROR(L"❌ Proprietatea 'syntax_path' poate fi setată doar pe obiecte de tip CodeView! ID: " + args[0].toWString());
                return vData{ 0LL };
            }

            // 2. Apelăm metoda specifică din vCodeView. Aceasta va încărca XML-ul și va face highlight instant.
            std::wstring sPath = value.toWString();
            codeView->setSyntaxPath(sPath);

            LOG_SUCCESS(L"🎨 [Syntax] Sintaxa nouă a fost încărcată cu succes din: " + sPath + L" pentru editorul: " + wId);
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

        if (propName == L"anchor") {
            Anchor a = ctrl->getAnchor();
            if (a == Anchor::CENTER) return vData(L"CENTER");

            std::wstring res = L"";
            if (hasFlag(a, Anchor::LEFT))   res += L"LEFT|";
            if (hasFlag(a, Anchor::RIGHT))  res += L"RIGHT|";
            if (hasFlag(a, Anchor::TOP))    res += L"TOP|";
            if (hasFlag(a, Anchor::BOTTOM)) res += L"BOTTOM|";
            if (hasFlag(a, Anchor::CENTER_H)) res += L"CENTER_H|";
            if (hasFlag(a, Anchor::CENTER_V)) res += L"CENTER_V|";

            if (!res.empty() && res.back() == L'|') res.pop_back(); // Curățăm ultimul separator
            return res.empty() ? vData(L"NONE") : vData(res);
        }
        if (propName == L"width_mode") {
            SizeMode mode = ctrl->getWidthMode();
            if (mode == SizeMode::FIXED)   return vData(L"FIXED");
            if (mode == SizeMode::FILL)    return vData(L"FILL");
            if (mode == SizeMode::AUTO)    return vData(L"AUTO");
            if (mode == SizeMode::PERCENT) return vData(L"PERCENT");
            return vData(L"UNKNOWN");
        }
        if (propName == L"height_mode") {
            SizeMode mode = ctrl->getHeightMode();
            if (mode == SizeMode::FIXED)   return vData(L"FIXED");
            if (mode == SizeMode::FILL)    return vData(L"FILL");
            if (mode == SizeMode::AUTO)    return vData(L"AUTO");
            if (mode == SizeMode::PERCENT) return vData(L"PERCENT");
            return vData(L"UNKNOWN");
        }
        if (propName == L"margin") {
            // Returnăm marginile către script sub formă de Array Oli compus din cele 4 valori: [Left, Top, Right, Bottom]
            auto marginArray = std::make_shared<std::vector<vData>>();
            marginArray->push_back(vData(static_cast<long long>(ctrl->getMarginLeft())));
            marginArray->push_back(vData(static_cast<long long>(ctrl->getMarginTop())));
            marginArray->push_back(vData(static_cast<long long>(ctrl->getMarginRight())));
            marginArray->push_back(vData(static_cast<long long>(ctrl->getMarginBottom())));
            return vData(marginArray);
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