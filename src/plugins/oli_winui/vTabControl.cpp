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
}



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
            SetWindowPos(pPage->getHandle(), NULL, -10000, -10000, 0, 0, SWP_HIDEWINDOW | SWP_NOSIZE);
        }
    }
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

    return vContainer::callMethod(methodName, args);
}