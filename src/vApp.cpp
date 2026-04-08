//#include "FontManager.hpp"
#include "vApp.hpp"
/*
#include "vPanel.hpp"
#include "vButton.hpp" // Necesită vButton
#include "vComboBox.hpp"
#include "vLabel.hpp"
#include "vEdit.hpp"
#include "vGrid.hpp"
#include "vDbGrid.hpp"
#include "vMenu.hpp"
#include "../dbConnection.hpp"
#include "../odbcConnection.hpp"

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

*/
// Inițializează pointerul static al instanței în afara clasei.
vApp* vApp::s_instance = nullptr;

// --- Constructor ---
vApp::vApp(HINSTANCE hInstance, RunMode mode)
    : m_instance(hInstance) {
    s_instance = this;
    m_runMode = mode;
}

// --- Metoda Run ---
int vApp::run(int nCmdShow) {
    if (!init()) {
        ConsoleManager::getInstance().log(L"[ERROR] Inițializarea aplicației a eșuat.");
        return -1;
    }
    if (m_runMode == RunMode::GUI) {
        /*
        HWND hwnd = getMainWindow();
        if (hwnd) {
            ShowWindow(hwnd, nCmdShow);
            UpdateWindow(hwnd);
        }
        else {
            ConsoleManager::getInstance().log(L"[ERROR] Handle-ul ferestrei principale este invalid după inițializare.");
            return -1;
        }
     
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            // 1. Obținem fereastra care deține focusul în mod activ (poate fi Main sau Dialogul)
            HWND hActive = GetActiveWindow();

            // 2. IsDialogMessage are nevoie de handle-ul ferestrei TOP-LEVEL active
            // pentru a procesa corect TAB între copiii acelei ferestre.
            if (hActive && IsDialogMessage(hActive, &msg)) {
                continue;
            }

            if (!IsDialogMessage(hwnd, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        return static_cast<int>(msg.wParam);   */
    }
    else if (m_runMode == RunMode::CONSOLE) {
        //LOG_INFO(L"[vApp::run] Rulează în modul consolă...");
       
        return 0;
    }

    else if (m_runMode == RunMode::SERVICE) {
        //LOG_INFO(L"[vApp::run] Rulează în modul service...");
        return 0;
    }

    return -1;
}

bool vApp::init() {
    // Inițializare comună (FontManager, ConsoleManager etc.)
    
    // ... Alte inițializări comune ...
    

    // Ramificarea logicii
    switch (m_runMode) {
    case RunMode::GUI:
        /*
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_USEREX_CLASSES | ICC_STANDARD_CLASSES | ICC_DATE_CLASSES;
        InitCommonControlsEx(&icex);
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        */
        return initGui(); // Apează inițializarea GUI
    case RunMode::CONSOLE:
        return initConsole(); // Apează inițializarea Console
    case RunMode::SERVICE:
        return initService(); // Apează inițializarea Service
    default:
        ConsoleManager::getInstance().log(L"[ERROR] Mod de rulare necunoscut.");
        return false;
    }
}

/*

// În vApp.cpp
LRESULT vApp::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    vWindow* pWindow = m_windowManager.getWindowByHandle(hwnd);
    if (!pWindow) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    // Inițializăm LRESULT cu un rezultat implicit
    LRESULT result = DefWindowProc(hwnd, msg, wParam, lParam);

    // Gestionează mesajele specifice la nivel de vApp
    switch (msg) {
    case WM_COMMAND: {
        int notificationCode = HIWORD(wParam);
        int win32Id = LOWORD(wParam);

        // Verifică dacă mesajul provine de la un meniu
        if (notificationCode == 0) {
            std::string itemId = ControlIdManager::getNameById(win32Id);
            if (!itemId.empty()) {
                ConsoleManager::getInstance().log(L"[vApp::handleMessage] Interceptat WM_COMMAND de la un meniu. ID: " + std::wstring(itemId.begin(), itemId.end()));
                this->getEventDispatcher().dispatch(itemId);
                result = 0; // Indică gestionarea mesajului de meniu.
            }
        }
        // Nu punem "break;" aici pentru a permite ca mesajele
        // de la controale (cu `notificationCode` != 0) să fie
        // procesate de `vWindow` mai jos.
        break;
    }

    case WM_DESTROY: {
        // Aici ar trebui să se ajungă doar pentru fereastra principală
        //if (pWindow->getId() == "main") {
        if(pWindow->isMainWindow()){
            ConsoleManager::getInstance().log(L"[vApp::handleMessage] Am primit WM_DESTROY pentru FEREASTRA PRINCIPALĂ. Se închide aplicația.");
            //shutdown();
            PostQuitMessage(0);
            result = 0; // Mesaj gestionat
        }
        break;
    }
    }

    // Deleagă mesajul către fereastra corespunzătoare, dar numai dacă nu a fost deja gestionat
    if (result != 0) {
        result = pWindow->handleMessage(hwnd, msg, wParam, lParam);
    }

    return result;
}
*/
/*
// --- Procedura Statică de Fereastră ---
// Acesta este punctul de intrare pentru mesajele WinAPI.
// Recuperază instanța vApp și deleagă mesajul.
LRESULT CALLBACK vApp::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	// Acest WndProc este setat pentru CLASA DE FEREASTRĂ PRINCIPALĂ (VAppMainWindowClass).
	// Acesta va fi apelat pentru mesaje destinate ferestrei principale.
	// Pentru controalele copil (panouri, butoane), WinAPI trimite mesaje PĂRINTELUI,
	// care apoi le deleagă mai departe.

	// La WM_NCCREATE (sau WM_CREATE pentru controale copil), lParam conține pointerul `this`.
	// Acest mecanism este folosit în vControl::StaticWndProc.
	// Aici, pentru fereastra principală, vom folosi `s_instance` pentru a obține `this`
	// și a delega.
	if (msg == WM_NCCREATE) {
		CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
		vApp* pApp = static_cast<vApp*>(pCreate->lpCreateParams);
		if (pApp) {
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pApp));
			// Aici nu este *nevoie* să apelăm handleMessage al lui pApp,
			// deoarece WndProc este deja în contextul lui pApp.
			// Putem să-l lăsăm să continue cu procesarea normală.
		}
	}

	// Recuperăm instanța vApp stocată în GWLP_USERDATA sau folosim s_instance.
	// Deoarece WndProc-ul acesta este atașat clasei de fereastră principale,
	// este mai simplu să folosim `s_instance` direct, deoarece este un singleton.
	// Alternativ, dacă `vApp` ar gestiona mai multe ferestre top-level (ceea ce nu e cazul aici),
	// atunci s-ar căuta instanța `vWindow` asociată cu `hwnd` și s-ar delega.

	// În contextul tău curent, `vApp::WndProc` este strict pentru FEREASTRA PRINCIPALĂ.
	// Prin urmare, delegăm la `s_instance->handleMessage`, iar `handleMessage`
	// se va ocupa de găsirea obiectului `vWindow` corespunzător `hwnd`-ului primit.

	if (s_instance) {
		return s_instance->handleMessage(hwnd, msg, wParam, lParam);
	}

	// Dacă s_instance nu este setat (ar trebui să fie deja setat în constructor),
	// sau pentru mesaje anterioare WM_NCCREATE/WM_CREATE unde GWLP_USERDATA nu e setat.
	return DefWindowProc(hwnd, msg, wParam, lParam);
}
*/

// --- Metoda Shutdown ---
void vApp::shutdown() {
    ConsoleManager::getInstance().log(L"[vApp::shutdown] Se inițiază procedura de închidere...");
    ConsoleManager::getInstance().clearExtraOutputs();
   /*/ HWND hMain = getMainWindow();

    if (hMain && IsWindow(hMain)) {
        ConsoleManager::getInstance().log(L"[vApp::shutdown] Distrug fereastra principală.");
        // Folosim DestroyWindow pentru a declanșa WM_DESTROY
        DestroyWindow(hMain);
    }
    else {
        ConsoleManager::getInstance().log(L"[vApp::shutdown] Nu am găsit fereastra principală, forțez quit.");
        PostQuitMessage(0);
    }
    */
    // Managerul se curăță după ce ferestrele au primit semnalul de distrugere
   // m_windowManager.shutdown();

    ConsoleManager::getInstance().log(L"[vApp::shutdown] Resurse eliberate.");
    ConsoleManager::getInstance().shutdown();
}
/*
// --- Metode Get Window ---
vWindow* vApp::getWindow(const std::string& id) {
	// Deleagă direct către metoda get a managerului de ferestre.
	return m_windowManager.get(id);
}
*/
/*
void vApp::addWindow(const std::string& id, std::unique_ptr<vWindow> window) {
    LOG_INFO(L"[vApp::addWindow] Se adaugă fereastra: " + str_to_wstr(id));
	m_windowManager.add(id, std::move(window));
    
	//ConsoleManager::getInstance().log(L"[vApp::addWindow] Fereastra cu ID '" + std::wstring(id.begin(), id.end()) + L"' a fost adăugată la manager.");
}
*/
/*
void vApp::addWindow(const std::string& id, std::unique_ptr<vWindow> window) {
    LOG_INFO(L"[vApp::addWindow] Încercare adăugare fereastră: " + str_to_wstr(id));

    // Verificăm dacă ID-ul există deja în manager
    if (m_windowManager.exists(id)) {
        LOG_WARNING(L"[vApp] ATENȚIE: Fereastra cu ID-ul '" + str_to_wstr(id) +
            L"' există deja. O eliminăm pe cea veche înainte de adăugare.");

        // Forțăm eliminarea celei vechi ACUM. 
        // Asta asigură că destructorul ei curăță EventDispatcher-ul 
        // ÎNAINTE ca noul obiect să fie înregistrat în manager.
        m_windowManager.remove(id);
    }

    m_windowManager.add(id, std::move(window));
    LOG_INFO(L"[vApp::addWindow] Fereastra '" + str_to_wstr(id) + L"' a fost înregistrată cu succes.");
}
*/
/*
HWND vApp::getMainWindow() {
    // 1. Încercăm varianta specifică (dacă există)
    vWindow* mainWin = m_windowManager.get("main");
    if (mainWin) {
        return mainWin->getHandle();
    }

    // 2. Fallback pe prima fereastră disponibilă
    vWindow* firstWin = m_windowManager.getFirstWindow();
    if (firstWin) {
        return firstWin->getHandle();
    }

    // 3. Dacă ajungem aici, chiar nu avem nicio fereastră
    LOG_ERROR(L"Managerul de ferestre este gol în getMainWindow().");
    return nullptr;
}
*/
/*
// --- Metoda de Test ---
void vApp::test() {
	ConsoleManager::getInstance().log(L"[vApp::test] Se declanșează evenimentul 'onClose' pentru fereastra principală.");
	vWindow* mainWindow = getWindow("main");
	if (mainWindow) {
	//	mainWindow->dispatch("onClose");
	}
	else {
		ConsoleManager::getInstance().log(L"[vApp::test] Fereastra principală nu a fost găsită pentru declanșarea 'onClose'.");
	}
}
*/
void vApp::startConsole() {
   ConsoleManager::getInstance().initialize(); 
   ConsoleManager::getInstance().setColor(FOREGROUND_GREEN);
   //ConsoleManager::getInstance().log(L"Consola inițializată! [AppInit] Începe inițializarea aplicației...");
   ConsoleManager::getInstance().resetColor();
   
}
/*
void vApp::removeWindow(const std::string& id) {
    if (id.empty()) return;

    if (m_windowManager.exists(id)) {
        LOG_DEBUG(L"[vApp] removeWindow: Eliberăm resursele pentru ID: " + str_to_wstr(id));

        // Luăm pointerul la fereastră pentru a-i închide handle-ul Win32 înainte de ștergerea obiectului C++
        vWindow* win = m_windowManager.get(id);
        if (win && win->getHandle() && IsWindow(win->getHandle())) {
            DestroyWindow(win->getHandle());
        }

        // Ștergerea din map va apela destructorul unique_ptr -> vWindow -> vContainer -> removeHandlers
        m_windowManager.remove(id);
    }
    else {
        LOG_DEBUG(L"[vApp] removeWindow: ID-ul '" + str_to_wstr(id) + L"' nu există, nimic de șters.");
    }
}
*/