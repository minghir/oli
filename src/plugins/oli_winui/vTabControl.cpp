#include "vTabControl.hpp"
#include "vGrid.hpp"
#include "stringUtils.hpp"
#include <windowsx.h>

vTabControl::vTabControl(
    HINSTANCE hInstance,
    const std::string& id,
    int x, int y, int width, int height, EventDispatcher& dispatcher
) : vContainer(hInstance, id, x, y, width, height, dispatcher)
{
   // ConsoleManager::getInstance().log(L"[vTabControl::Constructor] Called for ID: " + str_to_wstr(id));

    // Inițializare Common Controls (necesar pentru WC_TABCONTROL)
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);
}

void vTabControl::create(HWND parent) {
  //  LOG_DEBUG(L"[vTabControl::create] Creating for ID: " + str_to_wstr(m_id));

    if (!parent) return;

    // Aplicăm scalarea DPI înainte de creare (ca în vPanel)
    UINT parentDpi = GetDpiForWindow(parent);
    scale(parentDpi);
    

    m_handle = CreateWindowEx(
        WS_EX_CONTROLPARENT, // Permite navigarea prin tab în interiorul paginilor
        WC_TABCONTROL,
        L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, // Elimină TCS_BUTTONS dacă vrei aspect de tab-uri lipite
        getX(), getY(), getWidth(), getHeight(),
        parent,
        (HMENU)(uintptr_t)getWin32Id(),
        m_hInstance,
        this
    );

    if (!m_handle) {
        LOG_ERROR(L"[ERROR] vTabControl creation failed. Error: " + std::to_wstring(GetLastError()));
    }
    else {
        // Inițializăm fontul și rect-ul original (ca în vPanel)
        scaleFont(getCurrentDpi());
        GetClientRect(m_handle, &m_originalClientRect);

      //  LOG_SUCCESS(L"[vTabControl::create] Success. HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(m_handle)));
    }
}

/*
void vTabControl::addTabPage(const std::wstring& title, std::unique_ptr<vPanel> page) {
    if (!m_handle) return;

    // Inserare în controlul nativ
    int index = TabCtrl_GetItemCount(m_handle);
    TCITEMW tie = { 0 };
    tie.mask = TCIF_TEXT;
    tie.pszText = (LPWSTR)title.c_str();
    SendMessageW(m_handle, TCM_INSERTITEMW, index, (LPARAM)&tie);

    vPanel* pPage = page.get();
    std::string pageId = page->getId();

    this->addChild(pageId, std::move(page), m_handle);

    // Calculăm zona de afișare
    RECT rc = { 0, 0, m_width, m_height }; // Folosim m_width/m_height ale obiectului C++
    TabCtrl_AdjustRect(m_handle, FALSE, &rc);

    // 🔥 FIX: Dacă rc e prea mic, folosim dimensiunea tab-ului
    if (rc.right - rc.left <= 0) {
        rc = {0, 30, m_width, m_height}; 
    }

    // Poziționare și redimensionare
    pPage->moveAndResize(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
    
    // 🔥 FORȚĂM layout-ul imediat, ca să știe panel-ul că trebuie să întindă copiii
    pPage->applyLayout(); 

    m_pages.push_back({ title, pPage });

    if (index == 0) {
        pPage->show(SW_SHOW);
    } else {
        pPage->hide();
        SetWindowPos(pPage->getHandle(), NULL, -10000, -10000, 0, 0, SWP_HIDEWINDOW | SWP_NOSIZE);
    }
    RedrawWindow(m_handle, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}
*/
void vTabControl::addTabPage(const std::wstring& title, std::unique_ptr<vPanel> page) {
    if (!m_handle) return;

    // Inserare în controlul nativ de tip Tab
    int index = TabCtrl_GetItemCount(m_handle);
    TCITEMW tie = { 0 };
    tie.mask = TCIF_TEXT;
    tie.pszText = (LPWSTR)title.c_str();
    SendMessageW(m_handle, TCM_INSERTITEMW, index, (LPARAM)&tie);

    vPanel* pPage = page.get();
    std::string pageId = page->getId();

    // =================================================================
    // 🔥 FIXUL CRITIC #1: Îl ascundem IMEDIAT dacă nu este primul tab!
    // =================================================================
    if (index > 0) {
        pPage->hide(); // Îi tăiem vizibilitatea înainte de orice redimensionare
    }

    this->addChild(pageId, std::move(page), m_handle);

    // Calculăm zona de afișare corectă
    RECT rc = { 0, 0, m_width, m_height };
    TabCtrl_AdjustRect(m_handle, FALSE, &rc);

    if (rc.right - rc.left <= 0) {
        rc = { 0, 30, m_width, m_height };
    }

    // Acum redimensionarea este 100% sigură! Dacă index > 0, se face în background silențios
    pPage->moveAndResize(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
    pPage->applyLayout();

    m_pages.push_back({ title, pPage });

    // =================================================================
    // 🔥 FIXUL CRITIC #2: Logica de afișare/mutare curată
    // =================================================================
    if (index == 0) {
        pPage->show(SW_SHOW);
    }
    else {
        // Fiind deja ascuns vizual, îl mutăm în siguranță în afara ecranului
        SetWindowPos(pPage->getHandle(), NULL, -10000, -10000, 0, 0, SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE);
    }

    // Ștergem preventiv orice randare reziduală
    RedrawWindow(m_handle, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

/*
void vTabControl::switchPage(int index) {
    if (index < 0 || index >= (int)m_pages.size() || !m_handle) return;

    RECT rc = { 0, 0, m_width, m_height };
    TabCtrl_AdjustRect(m_handle, FALSE, &rc);
    int localW = rc.right - rc.left;
    int localH = rc.bottom - rc.top;

    for (int i = 0; i < (int)m_pages.size(); ++i) {
        vPanel* pPage = m_pages[i].panel;
        if (i == index) {
            // 🔥 Redimensionăm pagina la dimensiunea actuală a tab-ului
            pPage->moveAndResize(rc.left, rc.top, localW, localH);
            pPage->applyLayout(); 
            pPage->show(SW_SHOW);
        } else {
            pPage->hide();
            // Mutăm în afara ecranului
            RedrawWindow(m_handle, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        }
    }
    InvalidateRect(m_handle, nullptr, TRUE);
}
*/
void vTabControl::switchPage(int index) {
    if (index < 0 || index >= (int)m_pages.size() || !m_handle) return;

    RECT rc = { 0, 0, m_width, m_height };
    TabCtrl_AdjustRect(m_handle, FALSE, &rc);
    int localW = rc.right - rc.left;
    int localH = rc.bottom - rc.top;

    // 1. Mai întâi ASCUNDEM și mutăm toate celelalte pagini
    for (int i = 0; i < (int)m_pages.size(); ++i) {
        if (i != index) {
            vPanel* pPage = m_pages[i].panel;
            pPage->hide();
            SetWindowPos(pPage->getHandle(), NULL, -10000, -10000, 0, 0, SWP_HIDEWINDOW | SWP_NOSIZE);
        }
    }

    // 2. ABIA ACUM o poziționăm și o afișăm pe cea activă
    vPanel* activePage = m_pages[index].panel;
    activePage->moveAndResize(rc.left, rc.top, localW, localH);
    activePage->applyLayout();
    activePage->show(SW_SHOW);

    // Curățare finală de buffer grafic
    RedrawWindow(m_handle, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

int vTabControl::getSelectedIndex() const {
    return (m_handle) ? TabCtrl_GetCurSel(m_handle) : 0;
}

RECT vTabControl::getDisplayRect() {
    RECT rc = { 0, 0, getWidth(), getHeight() };
    if (m_handle) {
        TabCtrl_AdjustRect(m_handle, FALSE, &rc);
    }
    return rc;
}

vPanel* vTabControl::getCurrentPage() const {
    int index = getSelectedIndex();

    // Verificăm dacă indexul este valid (TabCtrl_GetCurSel returnează -1 dacă nu e nimic selectat)
    if (index >= 0 && index < static_cast<int>(m_pages.size())) {
        return m_pages[index].panel;
    }

    return nullptr;
}

LRESULT vTabControl::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

    // Tratăm notificările specifice (click pe tab)
    if (msg == WM_NOTIFY) {
        LPNMHDR lpnmhdr = reinterpret_cast<LPNMHDR>(lParam);
        if (lpnmhdr->code == TCN_SELCHANGE) {
           // LOG_DEBUG(L"Click Aicic!!!!");
            switchPage(TabCtrl_GetCurSel(m_handle));

            return 0;
        }
        else {
           // return getCurrentPage()->handleMessage(hwnd, msg, wParam, lParam);
        }
    }

    // Pentru WM_SIZE, procedăm ca în vPanel
    
    if (msg == WM_SIZE) {
        // 1. Updatează dimensiunile interne ale obiectului C++
        m_width = LOWORD(lParam);
        m_height = HIWORD(lParam);

        // 2. Lasă containerul să-și facă logica (scale, etc)
        LRESULT res = vContainer::handleMessage(hwnd, msg, wParam, lParam);

        // 3. REPOZIȚIONEAZĂ PAGINA CURENTĂ!
        // Fără asta, switchPage nu e apelat la resize, deci pg1 rămâne mic.
       
        switchPage(getSelectedIndex());
        return res;
    }
   

    return vContainer::handleMessage(hwnd, msg, wParam, lParam);
}

void vTabControl::moveAndResize(int x, int y, int width, int height) {
    // 1. Apelăm clasa de bază pentru a actualiza dimensiunile proprii ale tab-ului
    vContainer::moveAndResize(x, y, width, height);

    // 2. Notificăm pagina activă să se redimensioneze
    vPanel* currentPage = getCurrentPage();
    if (currentPage && currentPage->isVisible()) {
        RECT rc = { 0, 0, width, height };
        TabCtrl_AdjustRect(m_handle, FALSE, &rc); // Recalculăm zona de afișare
        
        currentPage->moveAndResize(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
        currentPage->applyLayout();
    }
}

bool vTabControl::setProperty(const std::wstring& name, const vData& value) {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    if (prop == L"selected_index") {
        this->switchPage(value.toInt());
        return true;
    }
    return vContainer::setProperty(name, value);
}

vData vTabControl::getProperty(const std::wstring& name) const {
    std::wstring prop = name;
    std::transform(prop.begin(), prop.end(), prop.begin(), ::tolower);

    if (prop == L"selected_index") return vData(this->getSelectedIndex());
    if (prop == L"tab_count") return vData((int)m_pages.size());
    if (prop == L"active_tab_id") {
        vPanel* cp = this->getCurrentPage();
        if (cp) {
            std::string id = cp->getId();
            return vData(std::wstring(id.begin(), id.end()));
        }
        return vData(L"");
    }
    
    return vContainer::getProperty(name);
}

bool vTabControl::callMethod(const std::wstring& methodName, const std::vector<vData>& args) {
    std::wstring method = methodName;
    std::transform(method.begin(), method.end(), method.begin(), ::tolower);

    if (method == L"add_tab_page") {
        if (args.size() < 2) return false;

        std::wstring wPanelId = args[1].toWString();
        std::string panelId(wPanelId.begin(), wPanelId.end());
        
        // Găsim panelul
        extern vControl* LocateAnyControl(const std::string& id);
        vControl* ctrl = LocateAnyControl(panelId);
        
        if (!ctrl) return false;

        vControl* oldParent = ctrl->getParent();
        if (oldParent) {
            // 1. EXTRAGEM pointerul fără să distrugem obiectul
            std::unique_ptr<vControl> extracted = oldParent->releaseChild(panelId);
            
            // 2. Facem un cast sigur. Știm că este vPanel.
            // .release() scoate raw pointer-ul din unique_ptr<vControl>
            vPanel* rawPanel = static_cast<vPanel*>(extracted.release());
            
            // 3. Îl adăugăm în tab, unde devine noul proprietar (unique_ptr)
            this->addTabPage(args[0].toWString(), std::unique_ptr<vPanel>(rawPanel));
        }
        return true;
    }
    
    if (method == L"switch_page") {
        if (args.empty()) return false;
        this->switchPage(args[0].toInt());
        return true;
    }

    // =================================================================
    // 🔥 IMPLEMENTARE ULTRA-ROBUSTĂ: REMOVE_TAB PRIN DEFERRED DELETION
    // =================================================================
    if (method == L"remove_tab" || method == L"remove_tab_page") {
        ConsoleManager::getInstance().log(L"[remove_tab] ---> A fost apelată funcția.");

        int index = this->getSelectedIndex();
        if (!args.empty()) index = args[0].toInt();

        if (index >= 0 && index < (int)m_pages.size()) {
            HWND hPageWnd = m_pages[index].panel->getHandle();
            std::string childId = m_pages[index].panel->getId();
            std::wstring wChildId(childId.begin(), childId.end());

            // Pasul 1: Ștergem tab-ul vizual din bara nativă de sus a Win32
            TabCtrl_DeleteItem(m_handle, index);

            // Pasul 2: Distrugem FIZIC și instantaneu ierarhia de ferestre din Windows
            if (hPageWnd && IsWindow(hPageWnd)) {
                // Rupem legătura directă cu WndProc pentru a ignora mesajele reziduale de distrugere
                ::SetWindowLongPtrW(hPageWnd, GWLP_USERDATA, 0);
                ::DestroyWindow(hPageWnd);
            }
            ConsoleManager::getInstance().log(L"[remove_tab] Pas 2: Ferestrele Win32 au fost distruse curat.");

            // Pasul 3: Îl scoatem din vectorul nostru local de pagini active
            m_pages.erase(m_pages.begin() + index);

            // Pasul 4: Extragem unique_ptr-ul din structura de containere a framework-ului
            auto destroyedChild = this->releaseChild(childId);
            ConsoleManager::getInstance().log(L"[remove_tab] Pas 4: Controlul a fost extras din ierarhia C++.");

            // Pasul 5: 🔥 TRUCUL ANTI-CRASH (Tehnica Zombie)
            // Mutăm obiectul într-un container static care îl ține în viață în background.
            // Evităm apelul de destructor (.reset()) acum, eliminând riscul de iterator invalidation!
            static std::vector<std::unique_ptr<vControl>> s_zombieGarbageCollector;
            if (destroyedChild) {
                s_zombieGarbageCollector.push_back(std::move(destroyedChild));
            }
            ConsoleManager::getInstance().log(L"[remove_tab] Pas 5: Obiectul C++ a fost parcat în siguranță în Zombie List.");

            // Pasul 6: Dacă au mai rămas tab-uri deschise, mutăm focusul pe următorul
            int count = TabCtrl_GetItemCount(m_handle);
            if (count > 0) {
                int newSel = (index >= count) ? count - 1 : index;
                TabCtrl_SetCurSel(m_handle, newSel);
                this->switchPage(newSel);
            }

            // Pasul 7: Redesenăm curat ecranul
            RedrawWindow(m_handle, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
            ConsoleManager::getInstance().log(L"[remove_tab] Succes total! Tab închis fără crash.");

            return true;
        }
        return false;
    }

    return vContainer::callMethod(methodName, args);
}