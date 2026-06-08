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
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | TCS_OWNERDRAWFIXED, // Elimină TCS_BUTTONS dacă vrei aspect de tab-uri lipite
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


void vTabControl::addTabPage(const std::wstring& title, std::unique_ptr<vControl> page) {
    if (!m_handle) return;

    int index = TabCtrl_GetItemCount(m_handle);
    TCITEMW tie = { 0 };
    tie.mask = TCIF_TEXT;

    // 🔥 FIX DE DESIGN: Adăugăm spații la finalul titlului doar pentru afișarea în Win32.
    // Asta va lărgi automat tab-ul și va împinge "x"-ul mai la dreapta, lăsând textul liber.
    std::wstring paddedTitle = title + L"     ";
    tie.pszText = (LPWSTR)paddedTitle.c_str();

    SendMessageW(m_handle, TCM_INSERTITEMW, index, (LPARAM)&tie);

    vControl* pPage = page.get();
    std::string pageId = page->getId();

    if (index > 0) {
        pPage->hide();
    }

    this->addChild(pageId, std::move(page), m_handle);

    RECT rc = { 0, 0, m_width, m_height };
    TabCtrl_AdjustRect(m_handle, FALSE, &rc);

    if (rc.right - rc.left <= 0) {
        rc = { 0, 30, m_width, m_height };
    }

    pPage->moveAndResize(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);

    if (auto* container = dynamic_cast<vContainer*>(pPage)) {
        container->applyLayout();
    }

    // 🔥 IMPORTANT: Aici salvăm 'title' cel original (fără spații) ca să rămână curat în logica motorului!
    m_pages.push_back({ title, pPage });

    if (index == 0) {
        pPage->show(SW_SHOW);
    }
    else {
        SetWindowPos(pPage->getHandle(), NULL, -10000, -10000, 0, 0, SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE);
    }

    RedrawWindow(m_handle, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}


void vTabControl::switchPage(int index) {
    if (index < 0 || index >= (int)m_pages.size() || !m_handle) return;

    TabCtrl_SetCurSel(m_handle, index);

    RECT rc = { 0, 0, m_width, m_height };
    TabCtrl_AdjustRect(m_handle, FALSE, &rc);
    int localW = rc.right - rc.left;
    int localH = rc.bottom - rc.top;

    for (int i = 0; i < (int)m_pages.size(); ++i) {
        if (i != index) {
            vControl* pPage = m_pages[i].panel;
            pPage->hide();
            SetWindowPos(pPage->getHandle(), NULL, -10000, -10000, 0, 0, SWP_HIDEWINDOW | SWP_NOSIZE);
        }
    }

    vControl* activePage = m_pages[index].panel;
    activePage->moveAndResize(rc.left, rc.top, localW, localH);

    // 🔥 Verificare polimorfică pentru layout
    if (auto* container = dynamic_cast<vContainer*>(activePage)) {
        container->applyLayout();
    }
    activePage->show(SW_SHOW);

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

vControl* vTabControl::getCurrentPage() const {
    int index = getSelectedIndex();
    if (index >= 0 && index < static_cast<int>(m_pages.size())) {
        return m_pages[index].panel;
    }
    return nullptr;
}

LRESULT vTabControl::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	if (msg == WM_DRAWITEM) {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlType == ODT_TAB) {
            drawTab(dis);
            return TRUE;
        }
    }

    // Tratăm notificările specifice (click pe tab)
    if (msg == WM_NOTIFY) {
        LPNMHDR lpnmhdr = reinterpret_cast<LPNMHDR>(lParam);
        if (lpnmhdr->code == TCN_SELCHANGE) {
           // LOG_DEBUG(L"Click Aicic!!!!");
            switchPage(TabCtrl_GetCurSel(m_handle));
            m_dispatcher.dispatch("tab_changed", m_id);
            return 0;
        }
		if (lpnmhdr->code == NM_CLICK) {
			DWORD pos = GetMessagePos();
			POINT pt = { GET_X_LPARAM(pos), GET_Y_LPARAM(pos) };
			ScreenToClient(m_handle, &pt);

			TCHITTESTINFO hit = { 0 };
			hit.pt = pt;
			int index = TabCtrl_HitTest(m_handle, &hit);

			if (index != -1) {
				// Calculează rect-ul tab-ului pentru a vedea dacă X-ul a fost atins
				RECT tabRect;
				TabCtrl_GetItemRect(m_handle, index, &tabRect);
				
				// Dacă punctul e în dreapta (unde am desenat X-ul)
				if (pt.x > tabRect.right - 20) {
					// Aici declanșezi ștergerea
					//this->callMethod(L"remove_tab", { vData((long long)index) });
					std::string tabId = m_pages[index].panel->getId();
					m_dispatcher.dispatch("close_tab", m_id, tabId);
					return 0;
				}
			}
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
    vContainer::moveAndResize(x, y, width, height);

    vControl* currentPage = getCurrentPage();
    if (currentPage && currentPage->isVisible()) {
        RECT rc = { 0, 0, width, height };
        TabCtrl_AdjustRect(m_handle, FALSE, &rc);

        currentPage->moveAndResize(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);

        if (auto* container = dynamic_cast<vContainer*>(currentPage)) {
            container->applyLayout();
        }
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
        // 🔥 FIX: Schimbat din vPanel* în vControl* pentru a se potrivi cu noua semnătură
        vControl* cp = this->getCurrentPage();
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

    if (method == L"select_tab_by_id" || method == L"activate_tab_by_id") {
        if (args.empty()) return false;
        std::wstring wTargetId = args[0].toWString();
        std::string targetId(wTargetId.begin(), wTargetId.end());

        // Căutăm în vectorul de pagini dacă există deja una cu acest ID
        for (int i = 0; i < (int)m_pages.size(); ++i) {
            if (m_pages[i].panel->getId() == targetId) {
                // 1. Schimbăm vizual pagina în container
                this->switchPage(i);
                // 2. Schimbăm selecția în bara nativă Win32 din capsulă
                TabCtrl_SetCurSel(m_handle, i);

                ConsoleManager::getInstance().log(L"[vTabControl] Tab-ul existent a fost găsit și activat direct.");
                return true; // Returnăm succes către scriptul Oli
            }
        }
        return false; // Tab-ul nu este deschis, trebuie creat
    }


    if (method == L"add_tab_page") {
        if (args.size() < 2) return false;

        std::wstring wPanelId = args[1].toWString();
        std::string panelId(wPanelId.begin(), wPanelId.end());

        extern vControl* LocateAnyControl(const std::string & id);
        vControl* ctrl = LocateAnyControl(panelId);

        if (!ctrl) return false;

        vControl* oldParent = ctrl->getParent();
        if (oldParent) {
            // 🔥 PERFECT: Extragem instanța neatinsă, păstrându-și tipul exact (vCodeView)
            std::unique_ptr<vControl> extracted = oldParent->releaseChild(panelId);

            // O trimitem direct ca unique_ptr<vControl> către addTabPage! ZERO CAST-uri, ZERO UB!
            this->addTabPage(args[0].toWString(), std::move(extracted));
        }
        return true;
    }
    
    if (method == L"switch_page") {
        if (args.empty()) return false;
        this->switchPage(args[0].toInt());
        return true;
    }
	
	if (method == L"remove_tab_page_by_id") {
        if (args.empty()) return false;
        std::wstring wTargetId = args[0].toWString();
        std::string targetId(wTargetId.begin(), wTargetId.end());

        // Căutăm indexul tab-ului după ID cu gardă de protecție la pointeri
        for (int i = 0; i < (int)m_pages.size(); ++i) {
            if (m_pages[i].panel != nullptr && m_pages[i].panel->getId() == targetId) {
                return this->callMethod(L"remove_tab", { vData((long long)i) });
            }
        }
        return false;
    }

    if (method == L"remove_tab" || method == L"remove_tab_page") {
        int index = this->getSelectedIndex();
        if (!args.empty()) index = args[0].toInt();

        if (index >= 0 && index < (int)m_pages.size()) {
            HWND hPageWnd = m_pages[index].panel->getHandle();
            std::string childId = m_pages[index].panel->getId();

            // Pasul 1: Ștergem tab-ul vizual din bara nativă Win32
            TabCtrl_DeleteItem(m_handle, index);

            // Pasul 2: Distrugem fereastra Win32 direct
            if (hPageWnd && IsWindow(hPageWnd)) {
                // 🔥 ELIMINAT SetWindowLongPtr(..., 0)! 
                // Lăsăm obiectul să își primească mesajele de distrugere (WM_DESTROY) 
                // în deplină siguranță polimorfică, fiindcă e ținut în viață de Zombie List.
                ::DestroyWindow(hPageWnd);
            }

            // Pasul 3: Scoatem din vectorul local de pagini active
            m_pages.erase(m_pages.begin() + index);

            // Pasul 4: Extragem din structura de containere a framework-ului
            auto destroyedChild = this->releaseChild(childId);

            // Pasul 5: Îl parcăm în Zombie Garbage Collector pentru a evita invalidarea iteratorilor
            static std::vector<std::unique_ptr<vControl>> s_zombieGarbageCollector;
            if (destroyedChild) {
                s_zombieGarbageCollector.push_back(std::move(destroyedChild));
            }

            // Pasul 6: Focus pe următorul tab rămas
            int count = TabCtrl_GetItemCount(m_handle);
            if (count > 0) {
                int newSel = (index >= count) ? count - 1 : index;
                TabCtrl_SetCurSel(m_handle, newSel);
                this->switchPage(newSel);
            }

            // Pasul 7: Redesenare curată
            RedrawWindow(m_handle, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
            return true;
        }
        return false;
    }

	if (method == L"refresh") {
        this->refresh();
        return true;
    }

    if (method == L"set_tab_title_by_id") {
        if (args.size() < 2) return false;

        std::wstring wTabId = args[0].toWString();
        std::string tabId(wTabId.begin(), wTabId.end());
        std::wstring cleanTitle = args[1].toWString();
        std::wstring paddedTitle = cleanTitle + L"     "; // Păstrăm fixul de design pentru spațierea "x"-ului

        for (int i = 0; i < (int)m_pages.size(); ++i) {
            if (m_pages[i].panel && m_pages[i].panel->getId() == tabId) {
                m_pages[i].title = cleanTitle; // Actualizăm titlul curat în structura internă

                TCITEMW tie = { 0 };
                tie.mask = TCIF_TEXT;
                tie.pszText = (LPWSTR)paddedTitle.c_str();

                // Trimitem mesajul nativ Win32 pentru a redesena textul din tab
                SendMessageW(m_handle, TCM_SETITEMW, i, (LPARAM)&tie);

                RedrawWindow(m_handle, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
                return true;
            }
        }
        return false;
    }

    return vContainer::callMethod(methodName, args);
}

void vTabControl::refresh() {
    if (!m_handle || !IsWindow(m_handle)) return;

    RECT rc = { 0, 0, m_width, m_height };
    TabCtrl_AdjustRect(m_handle, FALSE, &rc);
    int localW = rc.right - rc.left;
    int localH = rc.bottom - rc.top;

    vControl* currentPage = getCurrentPage();
    if (currentPage) {
        currentPage->moveAndResize(rc.left, rc.top, localW, localH);
        if (auto* container = dynamic_cast<vContainer*>(currentPage)) {
            container->applyLayout();
        }
    }

    RedrawWindow(m_handle, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}


void vTabControl::drawTab(LPDRAWITEMSTRUCT dis) {
    HDC hdc = dis->hDC;
    RECT rect = dis->rcItem;
    int index = dis->itemID;

    // 1. Fundalul tab-ului
    FillRect(hdc, &rect, (HBRUSH)(COLOR_3DFACE + 1));

    // 2. Textul tab-ului
    TCITEMW item = { 0 };
    item.mask = TCIF_TEXT;
    wchar_t buffer[256];
    item.pszText = buffer;
    item.cchTextMax = 256;
    TabCtrl_GetItem(m_handle, index, &item);
    
    RECT textRect = rect;
    textRect.left += 5;
    DrawTextW(hdc, item.pszText, -1, &textRect, DT_SINGLELINE | DT_VCENTER);

    // 3. Desenează "X"-ul (în dreapta)
    RECT closeRect = rect;
    closeRect.left = closeRect.right - 20; // 20px lățime pentru buton
    
    // Desenăm un X simplu
    SetTextColor(hdc, RGB(0, 0, 0));
    DrawTextW(hdc, L"×", -1, &closeRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
}