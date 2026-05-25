#include "vWindow.hpp"
#include "vMenu.hpp" 
#include "vApp.hpp" 
#include "ConsoleManager.hpp"
#include "stringUtils.hpp"
// stringUtils.hpp -- Eliminat, corect dacă nu e utilizat.

// --- Constructor ---
vWindow::vWindow(HINSTANCE hInstance, const std::string& id, WindowType type, bool isMainWindow, EventDispatcher& dispatcher)
    : vContainer(hInstance, id, dispatcher), // Apelează constructorul clasei de bază vContainer.
    m_WindowType(type),
    //m_hInstance(hInstance),
    m_isMainWindow(isMainWindow) { // Inițializează noul membru
    m_ControlType = ControlType::Window;
    //ConsoleManager::getInstance().log(L"[vWindow::Constructor] Apelat pentru ID: " + std::wstring(id.begin(), id.end()) + (isMainWindow ? L" (Fereastră Principală)" : L""));
}

vWindow::~vWindow() {
    if (m_isModal && m_hParentForModal) {
        EnableWindow(m_hParentForModal, TRUE);
    }
}

/*
// --- Metoda Create (detaliată) ---
bool vWindow::create(const std::wstring& className, const std::wstring& title,
    DWORD style, int x, int y, int w, int h,
    HWND parent, HMENU menu) {
		
		m_hInstance = GetModuleHandle(nullptr);
		
    
    DWORD dwExStyle = 0; // Variabilă nouă pentru stiluri extinse
   // LOG_WARNING()
    switch (m_WindowType) {
    case WindowType::StandardWindow:
        style |= WS_OVERLAPPEDWINDOW;
        break;

    case WindowType::DialogWindow:
        style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_BORDER;
        break;

    case WindowType::ToolWindow:
        // WS_SYSMENU aduce butonul X
        style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
        dwExStyle = WS_EX_TOOLWINDOW; // Mutat aici
        break;

    case WindowType::PopupWindow:
        style = WS_POPUP;
        break;
    }


    m_base_x = x;
    m_base_y = y;
    m_base_width = w;
    m_base_height = h;



    // Folosește versiunea EXW pentru siguranță maximă
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = vControl::StaticWndProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = className.c_str();
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    WNDCLASSEXW existingWc = { 0 };
    existingWc.cbSize = sizeof(WNDCLASSEXW);

    // Folosește GetClassInfoExW și RegisterClassExW
    if (!GetClassInfoExW(m_hInstance, className.c_str(), &existingWc)) {
        if (!RegisterClassExW(&wc)) {
            return false;
        }
    }
	


    // Creează fereastra WinAPI. `this` este pasat ca lpCreateParams.

   // double scaleFactor = static_cast<double>(getPrimaryMonitorDpi()) / 96.0;
    //int scaled_w = static_cast<int>(w * scaleFactor);
    //int scaled_h = static_cast<int>(h * scaleFactor);
    //UINT parentDpi = GetDpiForWindow(parent);
    // Apelează metoda de scalare cu DPI-ul obținut
    
	scale(GetDpiForSystem());
    
  //  ConsoleManager::getInstance().log(L"[ERROR] vWindow creation: Error creating panel HWND for ID '" + str_to_wstr(m_id) + L"'. Error code: " + std::to_wstring(GetLastError()));
  //  ConsoleManager::getInstance().log(L"[ERROR]\t\t\t x,y,width,height(" + std::to_wstring(getX()) + L"," + std::to_wstring(getY()) + L"," + std::to_wstring(getWidth()) + L"," + //std::to_wstring(getHeight()) + L")");
    

    m_handle = CreateWindowExW(
        dwExStyle, // Transmitem stilul extins aici!
        className.c_str(),
        title.c_str(),
        style | WS_CLIPCHILDREN,
        getX(), getY(), getWidth(), getHeight(),
        parent,
        menu,
        m_hInstance,
        this);
    if (!m_handle) {
        //ConsoleManager::getInstance().log(L"[ERROR] Creare vWindow: Eroare la crearea ferestrei '" + title + L"'. Cod eroare: " + std::to_wstring(GetLastError()));
		return false;
    }
    else {
        centerWindow();
        //ConsoleManager::getInstance().log(L"[vWindow::create] Fereastra '" + title + L"' (ID: " + std::wstring(m_id.begin(), m_id.end()) + L") a fost creată cu succes. HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(m_handle)));
    }
	scale(GetDpiForSystem());
    return m_handle != nullptr;
}
*/
bool vWindow::create(const std::wstring& className, const std::wstring& title,
    DWORD style, int x, int y, int w, int h,
    HWND parent, HMENU menu) {
    
    m_hInstance = GetModuleHandle(nullptr);
    
    // 1. Determinare stiluri extinse
    DWORD dwExStyle = 0;
    switch (m_WindowType) {
        case WindowType::StandardWindow: style |= WS_OVERLAPPEDWINDOW; break;
        case WindowType::DialogWindow:   style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_BORDER; break;
        case WindowType::ToolWindow:     style = WS_POPUP | WS_CAPTION | WS_SYSMENU; dwExStyle = WS_EX_TOOLWINDOW; break;
        case WindowType::PopupWindow:    style = WS_POPUP; break;
    }

    m_base_x = x;
    m_base_y = y;
    m_base_width = w;
    m_base_height = h;

    // 2. Înregistrare clasă folosind structuri Unicode (W)
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = vControl::StaticWndProc; // Asigură-te că aici apelezi DefWindowProcW!
    wc.hInstance = m_hInstance;
    wc.lpszClassName = className.c_str();
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    WNDCLASSEXW existingWc = { 0 };
    existingWc.cbSize = sizeof(WNDCLASSEXW);

    if (!GetClassInfoExW(m_hInstance, className.c_str(), &existingWc)) {
        if (!RegisterClassExW(&wc)) {
            LOG_ERROR(L"[vWindow::create] RegisterClassExW a eșuat. Cod: " + std::to_wstring(GetLastError()));
            return false;
        }
    }

    // 3. Scalare DPI (Păstrată conform logicii tale)
    scale(GetDpiForSystem());

    // 4. Creare fereastră Unicode (W)
    m_handle = CreateWindowExW(
        dwExStyle,
        className.c_str(),
        title.c_str(), // title este std::wstring, perfect pentru W
        style | WS_CLIPCHILDREN,
        getX(), getY(), getWidth(), getHeight(),
        parent,
        menu,
        m_hInstance,
        this // `this` este trimis ca lParam in WM_CREATE
    );

    if (!m_handle) {
        LOG_ERROR(L"[vWindow::create] CreateWindowExW a eșuat. Cod: " + std::to_wstring(GetLastError()));
        return false;
    }

    centerWindow();
    scale(GetDpiForSystem());

    return true;
}


// --- Gestionare Mesaje (handleMessage) ---
LRESULT vWindow::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Mesajele de log au fost comentate, dar sunt utile pentru debug.
    /*
     ConsoleManager::getInstance().log(L"[vWindow::handleMessage] Mesaj: " + std::to_wstring(msg) +
          L", HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(hwnd)) +
          L", ID Fereastră: " + std::wstring(m_id.begin(), m_id.end()));
    */
    switch (msg) {

    case WM_NOTIFY: {
        LPNMHDR nm = (LPNMHDR)lParam;
        // În loc de findChildByHandle, folosim noua metodă care respectă arhitectura ta
        if (this->handleChildNotify(nm, msg, wParam, lParam)) {
            return 0; // Mesajul a fost gestionat
        }
        break;
    }
   
    case WM_KEYDOWN: {
        // VK_ESCAPE este codul pentru tasta Escape
        if (wParam == VK_ESCAPE) {
            // Închidem doar dacă este modală sau dacă decidem că 
            // orice fereastră care nu e principală se poate închide la ESC
            if (m_isModal || !m_isMainWindow) {
                //ConsoleManager::getInstance().log(L"[vWindow::handleMessage] Tasta ESC apăsată. Închid fereastra ID: " + str_to_wstr(m_id));
                this->hide();
                return 0;
            }
        }
        break;
    }

    case WM_COMMAND: {
        int notificationCode = HIWORD(wParam);
        int win32Id = LOWORD(wParam);
        //LOG_DEBUG(L"[vPanel::handleMessage] Received WM_COMMAND! Source ID: " + std::to_wstring(win32Id));// +L":" + str_to_wstr(control->getId()));
        // 1. Verifică dacă mesajul provine de la un MENIU
        if (lParam == 0) { // lParam este 0 pentru meniuri și acceleratori
            if (win32Id == IDCANCEL && !m_isMainWindow) {
                this->hide();
                return 0;
            }

            std::string menuItemId = ControlIdManager::getNameById(win32Id);
            LOG_DEBUG(L"[vWindow::handleMessage] Mesaj WM_COMMAND de la un MENIU/ACCELERATOR. ID: " + std::to_wstring(win32Id) + L":" + str_to_wstr(menuItemId));
            //getEventDispatcher().dispatch(menuItemId);
			getEventDispatcher().dispatch(menuItemId, m_id);
            //m_appDispatcher->dispatch(menuItemId);
           // ConsoleManager::getInstance().log(L"[vWindow::handleMessage] A fost emis un eveniment de meniu: " + str_to_wstr(menuItemId));
            // Mesajul a fost procesat, nu mai este necesar să-l pasăm mai departe.

            return 0;
            
        }

        // 2. Dacă nu este de la un meniu, verifică dacă provine de la un control copil
        else { // lParam este HWND-ul controlului copil
           // ConsoleManager::getInstance().log(L"[vWindow::handleMessage] Mesaj WM_COMMAND de la un CONTROL. ID: " + std::to_wstring(win32Id));

            if (handleChildCommand(win32Id, msg, wParam, lParam)) {
                return 0; // Mesajul a fost gestionat de un control copil.
            }
        }

        break;
    }
    case WM_CLOSE: {
        this->hide();

        // Dacă este fereastra principală, trebuie totuși să închidem aplicația
        if (m_isMainWindow) {
            DestroyWindow(hwnd);
        }
        return 0;
    }
    
    case WM_DESTROY: {
        // Când fereastra este pe cale să fie distrusă.
        ConsoleManager::getInstance().log(L"[vWindow::handleMessage] Primit WM_DESTROY. Fereastră ID: " + std::wstring(m_id.begin(), m_id.end()));

        // --- MODIFICARE CRUCIALĂ AICI ---
        // Postăm WM_QUIT DOAR dacă această fereastră este fereastra principală a aplicației.
        if (m_isMainWindow) {
           //ConsoleManager::getInstance().log(L"[vWindow::handleMessage] Aceasta este fereastra principală. Se postează WM_QUIT.");
            PostQuitMessage(0); // Semnalizează buclei de mesaje din `vApp::run` să se termine.
        }
        else {
            //ConsoleManager::getInstance().log(L"[vWindow::handleMessage] Aceasta NU este fereastra principală. NU se postează WM_QUIT.");
        }
        return 0; // Mesaj gestionat.
    }
    case WM_SIZE: {
        int newWidth = LOWORD(lParam);
        int newHeight = HIWORD(lParam);

        if (newWidth > 0 && newHeight > 0) {
            m_width = newWidth;
            m_height = newHeight;

            // În loc de m_layoutStrategy->applyLayout(*this);
            this->applyLayout();
        }
        return 0;
    }
    case WM_KILLFOCUS: {
                    //ConsoleManager::getInstance().log(L"[vWindow::handleMessage] Am pierdut focusul.");
                    // Poți apela o metodă precum onFocusLost() dacă ai definit una
                    return TRUE;
                }

    /*
    case WM_DPICHANGED: {

        int newDpi = HIWORD(wParam);
        RECT* suggested = (RECT*)lParam;


        //LOG_DEBUG(L"vWindow::handleMessage : AM PRIMIT WM_DPICHANGED incep scalarea la:" + to_wstring<int>(newDpi));

        SetWindowPos(hwnd, NULL,
            suggested->left,
            suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);


      

        // 3. Propagăm noul DPI în toată ierarhia (fonturi, coloane grid, etc.)
        this->scale(newDpi);

        // 4. Forțăm layout-ul să se recalculeze pentru noile dimensiuni și noul DPI
        //this->applyLayout();

        return 0;
    }
    */

    case WM_DPICHANGED: {
        int newDpi = HIWORD(wParam);
        RECT* suggested = (RECT*)lParam;

        // 1. SETĂM POZIȚIA SUGERATĂ DE WINDOWS (Esențial pentru a nu "sări" înapoi)
        SetWindowPos(hwnd, NULL,
            suggested->left,
            suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);

        // 2. ACTUALIZĂM DPI-UL ÎN IERARHIE
        // ATENȚIE: Verifică în vControl::scale să NU apelezi SetWindowPos cu SWP_NOMOVE = false 
        // pentru obiectele de tip Window, altfel vei muta fereastra la coordonatele ei relative (0,0 sau vechiul x,y)
        this->scale(newDpi);

        return 0;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        if (m_minWidth > 0) mmi->ptMinTrackSize.x = m_minWidth;
        if (m_minHeight > 0) mmi->ptMinTrackSize.y = m_minHeight;
        if (m_maxWidth < 32767) mmi->ptMaxTrackSize.x = m_maxWidth;
        if (m_maxHeight < 32767) mmi->ptMaxTrackSize.y = m_maxHeight;
        return 0;
    }

    default:
        // Pentru orice alt mesaj pe care vWindow nu-l gestionează direct,
        // se va continua la apelul clasei de bază de mai jos.
        break;
    }

    // IMPORTANT: Pasează mesajul către implementarea clasei de bază (vContainer),
    // care, la rândul ei, va apela vControl::handleMessage, care în cele din urmă apelează DefWindowProc
    // pentru mesaje negestionate explicit în ierarhie.
    return vContainer::handleMessage(hwnd, msg, wParam, lParam);
}


/*
void vWindow::setMenu(vMenu* pMenu) {
    if (!m_handle || !IsWindow(m_handle)) return;
    if (!pMenu || !pMenu->getHandle()) return;

    // Atașăm meniul ferestrei
    if (SetMenu(m_handle, pMenu->getHandle())) {
        // 🔥 ESENȚIAL: Forțăm fereastra să redeseneze bara de meniu
        DrawMenuBar(m_handle);
        // Dacă meniul tot nu apare, forțăm un resize minim care forțează recalcularea spațiului
        SetWindowPos(m_handle, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        
        LOG_DEBUG(L"[vWindow::setMenu] Meniu atașat și redesenat cu succes.");
    } else {
        LOG_ERROR(L"[vWindow::setMenu] SetMenu a esuat. Cod: " + std::to_wstring(GetLastError()));
    }
}
*/

void vWindow::setMenu(vMenu* pMenu) {
    if (!m_handle || !IsWindow(m_handle)) {
        LOG_ERROR(L"[vWindow::setMenu] Fereastra este invalida!");
        return;
    }
    
    HMENU hMenuToSet = pMenu->getHandle();
    LOG_DEBUG(L"[vWindow::setMenu] Incerc setarea meniului cu HMENU: " + std::to_wstring(reinterpret_cast<uintptr_t>(hMenuToSet)));

    if (!hMenuToSet) {
        LOG_ERROR(L"[vWindow::setMenu] Meniul primit are handle NULL!");
        return;
    }

    if (SetMenu(m_handle, hMenuToSet)) {
        SetWindowPos(m_handle, NULL, 0, 0, 0, 0, 
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        DrawMenuBar(m_handle);
        LOG_DEBUG(L"[vWindow::setMenu] Meniu atașat cu succes.");
    } else {
        LOG_ERROR(L"[vWindow::setMenu] SetMenu a eșuat. Cod eroare Win32: " + std::to_wstring(GetLastError()));
    }
}

void vWindow::showModal() {
    m_isModal = true;

    // 1. Identificare părinte
    m_hParentForModal = GetWindow(m_handle, GW_OWNER);
    if (!m_hParentForModal) {
        m_hParentForModal = vApp::getAppInstance()->getMainWindow();
    }

    // 2. Blocare părinte
    if (m_hParentForModal && m_hParentForModal != m_handle) {
        EnableWindow(m_hParentForModal, FALSE);
    }

    centerWindow();
    ShowWindow(m_handle, SW_SHOW);
    SetForegroundWindow(m_handle);
    SetFocus(m_handle);

    // 3. BUCUCLA DE MESAJE (MODAL LOOP)
    MSG msg;
    while (m_isModal && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(m_handle, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    // Când m_isModal devine false (în hide()), ieșim din while și execuția continuă
}

void vWindow::hide() {
    if (m_isModal) {
       // ConsoleManager::getInstance().log(L"[vWindow::hide] Fereastra este MODALĂ. Încerc reactivare părinte.");
        if (m_hParentForModal) {
            EnableWindow(m_hParentForModal, TRUE);
            SetForegroundWindow(m_hParentForModal);
          //  ConsoleManager::getInstance().log(L"[vWindow::hide] Părinte reactivat.");
        }
        m_isModal = false;
    }
    ShowWindow(m_handle, SW_HIDE);
}


void vWindow::centerWindow() {
    if (!m_handle || !IsWindow(m_handle)) return;

    HWND hOwner = GetWindow(m_handle, GW_OWNER);
    RECT rcOwner, rcChild;
    GetWindowRect(m_handle, &rcChild);

    int width = rcChild.right - rcChild.left;
    int height = rcChild.bottom - rcChild.top;
    int x, y;

    if (hOwner && IsWindowVisible(hOwner)) {
        // Centrare față de fereastra părinte/owner
        GetWindowRect(hOwner, &rcOwner);
        x = rcOwner.left + ((rcOwner.right - rcOwner.left) - width) / 2;
        y = rcOwner.top + ((rcOwner.bottom - rcOwner.top) - height) / 2;
    }
    else {
        // Centrare pe ecranul pe care se află mouse-ul sau fereastra
        HMONITOR hMonitor = MonitorFromWindow(m_handle, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(hMonitor, &mi)) {
            x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - width) / 2;
            y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - height) / 2;
        }
        else {
            // Fallback pe desktop-ul principal
            x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
            y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
        }
    }

    SetWindowPos(m_handle, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

bool vWindow::setProperty(const std::wstring& name, const vData& value) {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    // 1. Schimbă titlul ferestrei
    if (prop == L"title") {
        this->setText(value.toWString()); 
        return true;
    }
    // 2. Schimbă starea de afișare modală
    else if (prop == L"modal") {
        if (value.toBool()) {
            this->showModal();
        } else {
            this->show(); 
        }
        return true;
    }
    // 🔥 3. PROPRIETATEA NOUĂ: Atașează meniul de bară
    else if (prop == L"menu") {
        std::wstring wMenuId = value.toWString();
        std::string menuId(wMenuId.begin(), wMenuId.end());

        // Căutăm meniul în ierarhia de copii a ferestrei curente
        vControl* ctrl = this->getChildRecursive(menuId);
        vMenu* pMenu = dynamic_cast<vMenu*>(ctrl);
        
        if (pMenu) {
            this->setMenu(pMenu); // Apelează funcția nativă Win32 SetMenu
            return true;
        }
        return false;
    }

    // Dacă nu este o proprietate unică de fereastră, o trimitem la vContainer
    return vContainer::setProperty(name, value);
}

vData vWindow::getProperty(const std::wstring& name) const {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    if (prop == L"title") {
        return vData(this->getText());
    }
    if (prop == L"is_main") {
        return vData(this->isMainWindow());
    }
    if (prop == L"is_modal") {
        return vData(this->isModal());
    }

    // Fallback către lanțul de moștenire
    return vContainer::getProperty(name);
}