#include "stringUtils.hpp"
#include "vControl.hpp"
#include "vContainer.hpp"
#include "ConsoleManager.hpp"
#include "ControlIdManager.hpp"
#include "FontManager.hpp"
#include "TooltipManager.hpp" 

#include <shellscalingapi.h>
#include <algorithm>



WNDPROC vControl::s_originalWndProc = NULL;


WNDPROC vControl::getOriginalWndProc() {
    return s_originalWndProc;
}
// --- Accesor pentru Win32 ID ---
int vControl::getWin32Id() const {
    return m_win32Id;
}


// --- Constructors ---
vControl::vControl(HINSTANCE hInst, const std::string& id, EventDispatcher& dispatcher)
    : m_id(id), m_handle(nullptr), m_win32Id(ControlIdManager::allocate(id)),
    m_base_x(0), m_base_y(0), m_base_width(0), m_base_height(0),
    m_x(0), m_y(0), m_width(0), m_height(0),
    m_dispatcher(dispatcher), m_parent(nullptr),
    m_hInstance(hInst),
    m_fontName(L"Segoe UI"), m_baseFontSize(12) {
        scale(m_currentDpi);
        scaleFont(m_currentDpi);
        //setBackgroundColor(GetSysColor(COLOR_BTNFACE));
        //LOG_DEBUG(L"vControl: new:" + str_to_wstr(m_id) + L"(" + std::to_wstring(m_win32Id) + L")");
}

vControl::vControl(HINSTANCE hInst, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher)
    : m_id(id), m_handle(nullptr), m_win32Id(ControlIdManager::allocate(id)),
    m_base_x(x), m_base_y(y), m_base_width(width), m_base_height(height),
    m_x(x), m_y(y), m_width(width), m_height(height),
    m_dispatcher(dispatcher), m_parent(nullptr),
    m_hInstance(hInst),
    m_fontName(L"Segoe UI"), m_baseFontSize(12) {
            scale(m_currentDpi);
            scaleFont(m_currentDpi);
            //setBackgroundColor(GetSysColor(COLOR_BTNFACE));
            //LOG_DEBUG(L"vControl: new:" + str_to_wstr(m_id) + L"(" + std::to_wstring(m_win32Id) + L")");
    }

// --- New DPI-related method implementations ---


// Metodă helper pentru a accesa dispatcher-ul
EventDispatcher& vControl::getEventDispatcher() const {
    return m_dispatcher;
}

// --- Destructor ---
vControl::~vControl() {
    // 1. Curățăm handler-ele doar dacă ID-ul e valid și nu suntem la finalul programului
    try {
        if (!m_id.empty()) {
            // Verificăm dacă dispatcher-ul mai este "acolo" 
            // (Uneori e mai bine să avem un pointer și să verificăm if(m_dispatcher))
            m_dispatcher.removeHandlers(m_id);
        }
    }
    catch (...) {
        // Dacă crapă aici, înseamnă că m_dispatcher nu mai există în memorie
    }

    // 2. Curățăm HWND-ul
    if (m_handle && IsWindow(m_handle)) {
        // Această linie este FOARTE importantă pentru a opri WndProc
        SetWindowLongPtr(m_handle, GWLP_USERDATA, 0);

        // Dacă este un control copil, Win32 îl va distruge oricum când părintele moare.
        // Putem folosi o distruge mai blândă:
        DestroyWindow(m_handle);
    }
    m_handle = nullptr;

    if (m_bgBrush) DeleteObject(m_bgBrush);
}

// --- Accesor pentru ID-ul intern ---
const std::string& vControl::getId() const {
    return m_id;
}

// --- Accesor pentru Handle-ul WinAPI ---
HWND vControl::getHandle() const {
    return m_handle;
}

// --- Metoda Show ---
void vControl::show(int cmdShow) {

    m_logicVisible = (cmdShow != SW_HIDE);

    if (m_handle) {
        ShowWindow(m_handle, cmdShow);

        // Dacă afișăm, forțăm desenarea imediată pentru un feeling "snappy"
        if (cmdShow != SW_HIDE) {
            UpdateWindow(m_handle);
        }
    }

    // Propagăm la copii (pentru că am stabilit că hide-ul a fost recursiv)
    for (auto& pair : m_children) {
        if (pair.second) {
            pair.second->show(cmdShow);
        }
    }
}

void vControl::hide() {
    //LOG_DEBUG(L"[vControl::hide] Apelat pentru ID: " + str_to_wstr(m_id));
    m_logicVisible = false;
    if (m_handle) {
        //LOG_DEBUG(L"   -> Ascund HWND: " + std::to_wstring((uintptr_t)m_handle) + L" pentru ID: " + str_to_wstr(m_id));
        ShowWindow(m_handle, SW_HIDE);
    }
    else {
        //LOG_ERROR(L"   -> [ERROR] m_handle este NULL pentru ID: " + str_to_wstr(m_id));
    }

    // Verificăm câți copii avem în listă
    //LOG_DEBUG(L"   -> Are " + std::to_wstring(m_children.size()) + L" copii.");

    for (auto& pair : m_children) {
        if (pair.second) {
            pair.second->hide();
        }
    }
}

bool vControl::isVisible() const {
    return m_handle && IsWindowVisible(m_handle);
}

bool vControl::isLogicVisible() const {
    return m_logicVisible;
}

// --- Adăugare Copil ---
void vControl::addChild(const std::string& id, std::unique_ptr<vControl> ctrl) {
    ctrl->m_id = id;
    ctrl->setParent(this);

   // ConsoleManager::getInstance().log(L"[vControl::addChild] Apel create pentru '" + std::wstring(id.begin(), id.end()) +
     //   L"' cu parent HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(m_handle)));

    // Verificare CRITICĂ: Părintele WinAPI (m_handle al controlului CURENT) trebuie să fie valid
    // înainte de a crea controlul copilului cu el ca părinte.
    if (m_handle == nullptr || !IsWindow(m_handle)) {
        ConsoleManager::getInstance().log(L"[ERROR] Nu se poate adăuga copilul '" + std::wstring(id.begin(), id.end()) + L"'. Părintele WinAPI (m_handle) nu este valid.");
        // Consideră aruncarea unei excepții aici dacă e o eroare fatală pentru logica aplicației.
        return;
    }

    // Aici se apelează metoda virtuală `create` a copilului,
    // care va crea handle-ul WinAPI real al copilului, folosind `m_handle` al părintelui.
    ctrl->create(this->m_handle);
    // SUGERAT: Verifică dacă create a avut succes înainte de a continua
    if (ctrl->getHandle() == nullptr && !ctrl->isSpacer()) {
        ConsoleManager::getInstance().log(L"[ERROR] Crearea HWND-ului pentru copilul '" + std::wstring(id.begin(), id.end()) + L"' a eșuat. Nu se adaugă la copii.");
        return; // Nu adăugăm un copil fără HWND valid
    }

    // NOU: Adaugă controlul în harta indexată de ID-ul Win32.
    if (ctrl->getHandle() != nullptr) {
        m_controlsByWin32Id[ctrl->getWin32Id()] = ctrl.get();
        ctrl->show();
    }
    // Adaugă unique_ptr-ul în harta copiilor, transferând proprietatea.
    //m_children[id] = std::move(ctrl);
    m_children.push_back(std::make_pair(id, std::move(ctrl)));

    // Forțează un layout update dacă suntem într-un container
    vContainer* container = dynamic_cast<vContainer*>(this);
    if (container) {
        container->applyLayout();
        RedrawWindow(container->getHandle(), NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_ERASE);
    }
}

void vControl::addChild(const std::string& id, std::unique_ptr<vControl> ctrl, HWND visualParent) {

    ctrl->setParent(this);

    if (visualParent == nullptr || !IsWindow(visualParent)) {
        LOG_ERROR(L"visualParent invalid pentru " + str_to_wstr(id));
        return;
    }

    // Creăm controlul pe părintele vizual specificat
    ctrl->create(visualParent);

    if (ctrl->getHandle() != nullptr) {
        m_controlsByWin32Id[ctrl->getWin32Id()] = ctrl.get();
        ctrl->show();
    }

    m_children.push_back(std::make_pair(id, std::move(ctrl)));

    // Re-layout
    vContainer* container = dynamic_cast<vContainer*>(this);
    if (container) {
        container->applyLayout();
        RedrawWindow(container->getHandle(), NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_ERASE);
    }
}

vControl* vControl::addChildWithReturn(const std::string& id, std::unique_ptr<vControl> ctrl) {
    vControl* ptr = ctrl.get();
    addChild(id, std::move(ctrl));
    return ptr;
}


// --- Obținere Copil ---
vControl* vControl::getChild(const std::string& id) {
    for (auto& pair : m_children) {
        if (pair.first == id) {
            return pair.second.get();
        }
    }
    return nullptr;
}

// --- Eliminare Copil ---
/*
void vControl::removeChild(const std::string& id) {
    // Șterge elementul din mapă, declanșând distrugerea unique_ptr-ului și a obiectului vControl.
    m_children.erase(id);
}
*/

void vControl::removeChild(const std::string& id) {
    // Folosim un iterator pentru a găsi elementul în vector
    auto it = std::find_if(m_children.begin(), m_children.end(),
        [&id](const std::pair<std::string, std::unique_ptr<vControl>>& pair) {
            return pair.first == id;
        });

    if (it != m_children.end()) {
        // Înainte de a șterge obiectul, scoate-l și din harta de ID-uri Win32
        int win32Id = it->second->getWin32Id();
        m_controlsByWin32Id.erase(win32Id);

        // Șterge elementul din vector. 
        // Asta va distruge unique_ptr-ul, care la rândul lui va apela destructorul vControl.
        m_children.erase(it);

      //  ConsoleManager::getInstance().log(L"[vControl] Copilul '" +
       //     std::wstring(id.begin(), id.end()) + L"' a fost eliminat.");
    }
}

std::unique_ptr<vControl> vControl::releaseChild(const std::string& id) {
    // Căutăm elementul
    auto it = std::find_if(m_children.begin(), m_children.end(),
        [&id](const std::pair<std::string, std::unique_ptr<vControl>>& pair) {
            return pair.first == id;
        });

    if (it != m_children.end()) {
        // Înainte de a scoate obiectul, curățăm map-ul de ID-uri
        int win32Id = it->second->getWin32Id();
        m_controlsByWin32Id.erase(win32Id);

        // 🔥 AICI E MAGIA: Mutăm ownership-ul din vector în acest pointer temporar
        std::unique_ptr<vControl> ptr = std::move(it->second);
        
        // Ștergem doar intrarea din vector (fără să distrugem obiectul, ptr-ul îl ține în viață)
        m_children.erase(it);
        
        // Returnăm obiectul viu
        return ptr; 
    }
    return nullptr;
}

// --- Obținere Copii ---
//const std::map<std::string, std::unique_ptr<vControl>>& vControl::getChildren() const {
//    return m_children;
//}

// --- Gestionare Mesaje ---
// Aceasta este metoda membru non-statică care primește mesajele WinAPI.
// Implementarea de bază deleagă mesajele către procedura implicită a ferestrei.
// Clasele derivate pot suprascrie această metodă pentru a gestiona mesaje specifice.
LRESULT vControl::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // IMPORTANT: Aceasta este metoda generală. Ar trebui să gestioneze
    // mesaje comune tuturor controalelor și să trimită mesajele neprocesate
    // către DefWindowProc.
    // Un aspect esențial aici este procesarea WM_COMMAND pentru butoane/controale.

    // Logică pentru WM_COMMAND (din discuția anterioară)
    


    if (msg == WM_COMMAND) {
        int controlID = LOWORD(wParam);
        // NOTIFICĂRI DE LA CONTROALE COPIL
        // Este important ca vControl să gestioneze WM_COMMAND dacă el este un părinte.
        // Aici trebuie să te decizi: tratezi WM_COMMAND în vControl (dacă este părinte)
        // sau doar în vContainer/vWindow? În general, WM_COMMAND este trimis părintelui.
        // Dacă ești în vControl, ar trebui să te gândești dacă ești părinte.
        // Pentru a evita duplicarea, `vContainer` ar trebui să aibă o logica `handleChildCommand`.
        // Daca acest `handleMessage` este apelat direct pe controlul copil (ex: button)
        // atunci acel control copil poate reactiona.

        // Revizuire: Mesajul WM_COMMAND este trimis ÎNTOTDEAUNA ferestrei PĂRINTE
        // a controlului care l-a generat.
        // Deci, logica pentru BN_CLICKED (și alte notificări de la controale copil)
        // **NU** ar trebui să fie aici, în `vControl::handleMessage` în general.
        // Ea ar trebui să fie în `vWindow::handleMessage` sau `vPanel::handleMessage`
        // (care sunt `vContainer`-e și, deci, părinți).
        // Acolo vei parcurge copiii (m_children) și vei găsi controlul corespunzător ID-ului
        // și vei apela `child->onClick()` pe el.

        // Prin urmare, eliminăm logica WM_COMMAND (BN_CLICKED) de AICI.
        // Ea aparține în `vContainer::handleChildCommand` sau direct în `vWindow/vPanel::handleMessage`.
    }

    // Exemplu de mesaj general de tratat în vControl (dacă ar fi cazul):
    // if (msg == WM_DESTROY) { /* Logică de cleanup comună */ }

    // Daca hwnd este valid, pasam la procedura implicita
   
    switch (msg) {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HWND hChild = (HWND)lParam;
        HDC hdc = (HDC)wParam;
        vControl* child = getChildByWin32Id(GetDlgCtrlID(hChild)); // Sau metoda ta de căutare

        if (child) {
            // --- MOȘTENIRE FONT ---
            // Folosim getEffectiveFont() care va urca până la cel mai apropiat părinte cu font setat
            SelectObject(hdc, child->getEffectiveFont());

            // --- MOȘTENIRE CULOARE TEXT ---
            SetTextColor(hdc, child->getEffectiveTextColor());

            // --- MOȘTENIRE FUNDAL ---
            HBRUSH hBr = child->getEffectiveBackgroundBrush();
            if (hBr) {
                SetBkMode(hdc, OPAQUE);
                SetBkColor(hdc, child->getEffectiveBackgroundColor());
                return (LRESULT)hBr;
            }
            else {
                SetBkMode(hdc, TRANSPARENT);
                return (LRESULT)GetStockObject(NULL_BRUSH);
            }
        }
        break;
    }
                           // 2. Mesaj primit de controlul respectiv pentru propriul fundal (ex: Panel sau Window)
    case WM_ERASEBKGND: {
        if (m_hasCustomBackground) {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, m_bgBrush);
            return 1; // Am șters fundalul manual
        }
        break;
    }
    }
  
   

    if (hwnd) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0; // Returnează 0 dacă hwnd nu este valid.
}
/*
// --- Procedura Statică de Fereastră (StaticWndProc) ---
LRESULT CALLBACK vControl::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    vControl* self = nullptr;

    if (msg == WM_NCCREATE) {
        LPCREATESTRUCT pcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        self = static_cast<vControl*>(pcs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) {
            self->m_handle = hwnd;
        }
    }
    else {
        self = reinterpret_cast<vControl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->handleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
*/
LRESULT CALLBACK vControl::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    vControl* self = reinterpret_cast<vControl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (!self && msg == WM_NCCREATE) {
        LPCREATESTRUCT pcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        self = static_cast<vControl*>(pcs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) {
            self->m_handle = hwnd;
        }
    }

    if (self) {
        return self->handleMessage(hwnd, msg, wParam, lParam);
    }

    // Foarte important: Pentru controalele cu subclassing, 
    // dacă nu avem 'self', lăsăm procedura originală să se ocupe, nu DefWindowProc direct.
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void vControl::onClick() {
   // ConsoleManager::getInstance().log(L"[vControl::onClick] Metoda onClick implicită apelată pentru ID: " + std::wstring(m_id.begin(), m_id.end()));
    LOG_DEBUG(L"DEBUG CLICK: Obiectul de tip " + str_to_wstr(typeid(*this).name()) +
        L" are m_id = '" + str_to_wstr(m_id) + L"'");

    if (m_onClickCallback) {
        m_onClickCallback();
    }

    // Este important sa folosim 'dispatch' deoarece 'trigger' nu este o metoda in EventDispatcher-ul tau.
    m_dispatcher.dispatch("click", m_id); // Declanșează evenimentul generic "click" prin EventDispatcher.
}




vControl* vControl::getChildByWin32Id(int win32Id) {
    // Caută controlul în lista de copii
    for (const auto& pair : m_children) {
        vControl* child = pair.second.get();
        if (child && child->getWin32Id() == win32Id) {
            return child;
        }
    }
    return nullptr;
}
/*
void vControl::setEnabled(bool enable) {
    m_enabled = enable;
    if (m_handle && IsWindow(m_handle)) {
        if (enable) {
           // LOG_SUCCESS(L"[vControl::setEnabled] Activare control ID: " + str_to_wstr(m_id));
        }
        else {
           // LOG_SUCCESS(L"[vControl::setEnabled] Dezactivare control ID: " + str_to_wstr(m_id));
        }
        // Folosește funcția WinAPI pentru a activa/dezactiva fereastra
        //LOG_WARNING(L"vControl::setEnabled: SETEZ starea pentru un handle null. ID: " + str_to_wstr(m_id));
        EnableWindow(m_handle, enable ? TRUE : FALSE);
        RedrawWindow(m_handle, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
    }
    else {
        LOG_WARNING(L"vControl::setEnabled: Tentativă de a seta starea pentru un handle null. ID: " + str_to_wstr(m_id));
    }
}
*/

void vControl::setEnabled(bool enable) {
    m_enabled = enable;
    if (m_handle && IsWindow(m_handle)) {
        // Dacă vrem să activăm un control, verificăm dacă părintele e activ
        if (enable && m_parent && m_parent->getHandle()) {
            if (!IsWindowEnabled(m_parent->getHandle())) {
                // LOG_WARNING(L"Atenție: Activez un copil al cărui părinte este DEZACTIVAT!");
            }
        }

        EnableWindow(m_handle, enable ? TRUE : FALSE);

        // Pentru Edit-uri, e mai bine să folosim ReadOnly decât Disabled
        if (m_ControlType == ControlType::Edit) {
            SendMessage(m_handle, EM_SETREADONLY, (WPARAM)!enable, 0);
        }

        InvalidateRect(m_handle, NULL, TRUE);
    }
}

void vControl::onKillFocus() {
  //  ConsoleManager::getInstance().log(L"[vControl::onKillFocus] A fost pierdut focusul de pe " + str_to_wstr(m_id));
    getEventDispatcher().dispatch("lost_focus", m_id);
}


void vControl::setTooltipText(const std::wstring& text) {
  //  ConsoleManager::getInstance().log(L"[vControl::setTooltipText] A fost setat m_tooltipText =  " + text + L" pentru id:" + str_to_wstr(m_id));
    m_tooltipText = text;
    TooltipManager::getInstance().addTooltip(m_handle, m_tooltipText);
    //TooltipManager& tooltipMgr = TooltipManager::getInstance();
    //tooltipMgr.addTooltip(m_handle, L"Apasă pentru a continua");


}

/*
void vControl::scale(int newDpi) {
    if (m_currentDpi == newDpi) return;

    // Recalculăm dimensiunile
    m_width = MulDiv(m_base_width, newDpi, 96);
    m_height = MulDiv(m_base_height, newDpi, 96);

    // REPOZIȚIONARE: Doar dacă NU este o fereastră principală (vWindow)
    // Controalele copil (butoane, paneluri) trebuie mutate, 
    // dar fereastra principală este deja mutată de WM_DPICHANGED
    if (getType() != ControlType::Window) {
        m_x = MulDiv(m_base_x, newDpi, 96);
        m_y = MulDiv(m_base_y, newDpi, 96);
    }

    m_currentDpi = newDpi;

    if (m_handle) {
        // Dacă e fereastră principală, schimbăm DOAR mărimea, nu și poziția (x, y)
        UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
        if (getType() == ControlType::Window) flags |= SWP_NOMOVE;

        SetWindowPos(m_handle, NULL, m_x, m_y, m_width, m_height, flags);
    }
}
*/
/*
void vControl::scale(int newDpi) {
    // Logica de return trebuie să fie atentă: 
    // chiar dacă DPI-ul e același, dacă e prima rulare, copiii tot trebuie scalați.
    //if (m_currentDpi == newDpi && m_handle != nullptr) return;
    //LOG_INFO(L"vControl::scale: scalez dimensiuni pentru: " + str_to_wstr(this->getId()));
    m_currentDpi = newDpi;

    m_width = MulDiv(m_base_width, newDpi, 96);
    m_height = MulDiv(m_base_height, newDpi, 96);

    if (getType() != ControlType::Window) {
        m_x = MulDiv(m_base_x, newDpi, 96);
        m_y = MulDiv(m_base_y, newDpi, 96);
    }

    if (m_hasCustomFont) {
        scaleFont(newDpi);
    }

    if (m_handle) {
        UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
        if (getType() == ControlType::Window) flags |= SWP_NOMOVE;
        SetWindowPos(m_handle, NULL, m_x, m_y, m_width, m_height, flags);
    }
    
}
*/

void vControl::scale(int newDpi) {
    m_currentDpi = newDpi;

    // Scalăm dimensiunile
    m_width = MulDiv(m_base_width, newDpi, 96);
    m_height = MulDiv(m_base_height, newDpi, 96);

    // Protecția coordonatelor pentru ferestre Top-Level
    if (m_ControlType == ControlType::Window) {
        // Dacă e fereastra mare, NU îi calculăm X și Y, le lăsăm pe cele de la Windows
        if (m_handle) {
            SetWindowPos(m_handle, NULL, 0, 0, m_width, m_height,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_FRAMECHANGED);
        }
    }
    else {
        // Doar pentru copii calculăm coordonatele relative
        m_x = MulDiv(m_base_x, newDpi, 96);
        m_y = MulDiv(m_base_y, newDpi, 96);
        if (m_handle) {
            SetWindowPos(m_handle, NULL, m_x, m_y, m_width, m_height,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
    }

    if (m_hasCustomFont) scaleFont(newDpi);

    // Propagarea recursivă (Atenție: vContainer::scale va fi apelat aici pentru sub-containere)
    for (auto& childPair : m_children) {
        if (childPair.second) {
            childPair.second->scale(newDpi);
        }
    }
}

int vControl::getX() const { return m_x; }
int vControl::getY() const { return m_y; }
int vControl::getWidth() const { return m_width; }
int vControl::getHeight() const { return m_height; }

int vControl::getCurrentDpi() const { return  m_currentDpi;  }

void vControl::resize() {
    // Verifică dacă handle-ul ferestrei este valid
    if (m_handle && IsWindow(m_handle)) {
        /*
        ConsoleManager::getInstance().log(L"ERROR[vControl::resize] Redimensionez controlul '" + str_to_wstr(m_id) + L"' la (" +
            std::to_wstring(m_x) + L", " + std::to_wstring(m_y) + L") cu dimensiuni " +
            std::to_wstring(m_width) + L"x" + std::to_wstring(m_height) + L".");
            */
        // Apelează funcția WinAPI pentru a redimensiona fereastra.
        MoveWindow(m_handle, m_x, m_y, m_width, m_height, TRUE);
    }
    else {
       // ConsoleManager::getInstance().log(L"[WARNING] vControl::resize: Tentativă de a redimensiona un handle invalid pentru ID: " + str_to_wstr(m_id));
    }
}

void vControl::scaleFont(int newDpi) {
    //LOG_INFO(L"vControl::scaleFont: scalez font pentru: " + str_to_wstr(this->getId()));
   // if (m_currentDpi == newDpi && m_hFont != nullptr) return;
    // Folosește atributele stocate pentru a cere fontul de la manager
    //HFONT hFont = FontManager::getInstance().getScaledFont(m_fontName, m_baseFontSize, newDpi);
    m_currentDpi = newDpi;
    // 1. Cerem fontul și ÎL SALVĂM în m_hFont
    //m_hFont = FontManager::getInstance().getScaledFont(m_fontName, m_baseFontSize, newDpi);
    m_hFont = FontManager::getInstance().getScaledFont(
        m_fontName,
        m_baseFontSize,
        newDpi,
        m_fontWeight,    // <--- TRIMITEM FW_BOLD (700)
        m_fontItalic,
        m_fontUnderline
    );
    // Setezi fontul pe controlul WinAPI
    if (m_handle && m_hFont) {
        //LOG_INFO(L"vControl::scaleFont: Pentru control:" + str_to_wstr(m_id) + L" setez font:" + m_fontName + L" la DPI: " + to_wstring<int>(newDpi));
        SendMessage(m_handle, WM_SETFONT, (WPARAM)m_hFont, TRUE);
    }
}

/*
void vControl::scaleFont(int newDpi) {
    m_currentDpi = newDpi;
    if (m_fontName.empty()) return;

    // Calculăm înălțimea logică (MulDiv e cea mai precisă metodă în Win32)
    int logicalHeight = -MulDiv(m_baseFontSize, newDpi, 72);

    // Cerem fontul de la manager folosind TOATE stilurile salvate în vControl
    m_hFont = FontManager::getInstance().getFont(
        m_fontName,
        logicalHeight,
        m_fontWeight,    // TRIMITEM GREUTATEA (ex: 700 pentru Bold)
        m_fontItalic,
        m_fontUnderline,
        false            // strikeout implicit false
    );

    if (m_handle && m_hFont) {
        SendMessage(m_handle, WM_SETFONT, (WPARAM)m_hFont, TRUE);
    }
}
*/

/*
// Supraincarcare pentru a permite setarea stilului
void vControl::setFont(const std::wstring& fontName, int baseFontSize, int weight, bool italic, bool underline) {
    // Pasul 1: Salvăm toate proprietățile în membrii clasei
    m_fontName = fontName;
    m_baseFontSize = baseFontSize;
    m_fontWeight = weight;    // Aici se salvează FW_BOLD (700)
    m_fontItalic = italic;
    m_fontUnderline = underline;

    // Pasul 2: Aplicăm fontul folosind DPI-ul curent
    // Chiar dacă fereastra nu e creată încă, datele rămân salvate în membri
    if (m_handle) {

        scaleFont(m_currentDpi);

        SendMessage(m_handle, WM_SETFONT, (WPARAM)m_hFont, TRUE);

        // IMPORTANT: Spunem tuturor copiilor să se redeseneze deoarece 
        // ei ar putea moșteni acest font nou
        InvalidateRect(m_handle, NULL, TRUE);
        for (auto& pair : m_children) {
            pair.second->update(); // Sau InvalidateRect pe handle-ul copilului
        }

        // Forțăm redesenarea pentru a vedea schimbarea imediat
       
        UpdateWindow(m_handle);
    }
}
*/
/*
void vControl::setFont(const std::wstring& fontName, int baseFontSize, int weight, bool italic, bool underline) {
    // 1. Marcăm că acest control are acum propriul său stil de font
    m_hasCustomFont = true;

    m_fontName = fontName;
    m_baseFontSize = baseFontSize;
    m_fontWeight = weight;
    m_fontItalic = italic;
    m_fontUnderline = underline;

    if (m_handle) {
        scaleFont(m_currentDpi); // Aceasta va crea m_hFont-ul real

        // Aplicăm vizual controlului curent
        SendMessage(m_handle, WM_SETFONT, (WPARAM)m_hFont, TRUE);

        // Notificăm copiii care ar putea moșteni acest font
        for (auto& pair : m_children) {
            // Doar copiii care NU au fontul lor custom vor fi afectați vizual
            if (!pair.second->m_hasCustomFont) {
                // Forțăm redesenarea copilului pentru a declanșa WM_CTLCOLOR... 
                // unde getEffectiveFont() va returna noul font al părintelui (this)
                InvalidateRect(pair.second->getHandle(), NULL, TRUE);
            }
        }

        UpdateWindow(m_handle);
    }
}
*/

void vControl::setFont(const std::wstring& fontName, int baseFontSize, int weight, bool italic, bool underline) {
    m_hasCustomFont = true;
    m_fontName = fontName;
    m_baseFontSize = baseFontSize;
    m_fontWeight = weight;
    m_fontItalic = italic;
    m_fontUnderline = underline;

    // DETERMINĂ DPI-ul REAL înainte de a scala
    if (m_handle) {
        m_currentDpi = GetDpiForWindow(m_handle);
    }
    else if (m_parent && m_parent->getHandle()) {
        m_currentDpi = GetDpiForWindow(m_parent->getHandle());
    }
    else {
        m_currentDpi = GetSystemDpiForProcess(GetCurrentProcess()); // Fallback
    }

    scaleFont(m_currentDpi);

    if (m_handle) {
        SendMessage(m_handle, WM_SETFONT, (WPARAM)m_hFont, TRUE);

        // Notificăm copiii
        for (auto& pair : m_children) {
            if (!pair.second->m_hasCustomFont) {
                // Dacă copilul moștenește fontul, trebuie să știe că s-a schimbat la părinte
                InvalidateRect(pair.second->getHandle(), NULL, TRUE);
            }
        }
    }
}


HFONT vControl::getFont() const {
    return m_hFont;
}


void vControl::update() {
    if (m_handle && IsWindow(m_handle)) {
        scaleFont(m_currentDpi);
        InvalidateRect(m_handle, NULL, TRUE);
        UpdateWindow(m_handle);
    }
}


void vControl::moveAndResize(int x, int y, int width, int height) {
    if (!m_handle) {
        // Logare: handle-ul nu este creat încă, doar actualizăm membrii interni
        m_x = x;
        m_y = y;
        m_width = width;
        m_height = height;
        /*
        if (m_handle) {
            // Parametrul TRUE forțează fereastra să se repicteze (WM_PAINT)
            MoveWindow(m_handle, x, y, width, height, TRUE);
        }
        else {
            LOG_WARNING(L"moveAndResize apelat pe un control fără HWND: " + str_to_wstr(m_id));
        }
        return;
        */
        return;
    }

    // Apelul WinAPI pentru mutare și redimensionare
    // SWP_NOZORDER menține ordinea Z a ferestrei

    BOOL success = SetWindowPos(
        m_handle,
        NULL,
        x, y,
        width, height,
        SWP_NOZORDER
    );

    if (success) {
        // Actualizează membrii interni DUPĂ ce operațiunea WinAPI a reușit
        m_x = x;
        m_y = y;
        m_width = width;
        m_height = height;
    }
    else {
        // Logare eroare WinAPI
        // ...
    }
}


bool vControl::setId(const std::string& newId) {
    // 1. Dacă ID-ul este același, considerăm operațiunea reușită (fără efort)
    if (m_id == newId) return true;

    // 2. Verificăm unicitatea în cadrul părintelui
    if (m_parent) {
        for (const auto& pair : m_parent->m_children) {
            if (pair.first == newId) {
                ConsoleManager::getInstance().log(L"[ERROR] setId: ID-ul '" +
                    std::wstring(newId.begin(), newId.end()) + L"' este deja ocupat!");
                return false; // Eșec: ID duplicat
            }
        }

        // 3. Actualizăm cheia în lista părintelui
        bool foundInParent = false;
        for (auto& pair : m_parent->m_children) {
            if (pair.first == m_id) {
                pair.first = newId;
                foundInParent = true;
                break;
            }
        }

        if (!foundInParent) {
            LOG_WARNING(L"[WARNING] setId: Controlul nu a fost găsit în m_children al părintelui.");
            // Continuăm totuși schimbarea ID-ului intern, 
            // dar faptul că nu era în listă e un semn de problemă de integritate.
        }
    }

    // 4. Actualizăm handler-ele în EventDispatcher
    // Este critic să facem asta înainte de a schimba m_id-ul propriu-zis
    m_dispatcher.renameControlHandlers(m_id, newId);

    // 5. Actualizăm ID-ul intern
    m_id = newId;

 //   ConsoleManager::getInstance().log(L"[vControl] ID schimbat cu succes în: " +
  //      std::wstring(newId.begin(), newId.end()));

    return true; // Succes!
}

void vControl::clearChildren() {
    // 1. Curățăm harta de căutare rapidă (Win32 IDs)
    // Deoarece m_children deține obiectele prin unique_ptr, 
    // pointerii din m_controlsByWin32Id vor deveni invalizi imediat.
    m_controlsByWin32Id.clear();

    // 2. Curățăm vectorul de copii
    // Această linie este crucială: distruge toate obiectele unique_ptr.
    // Destructorul fiecărui vControl va fi apelat, care la rândul lui:
    //   - Va apela m_dispatcher.removeHandlers(m_id)
    //   - Va apela DestroyWindow(m_handle)
    m_children.clear();

    // 3. Forțăm o redesenare a controlului părinte 
    // (pentru a șterge "urmele" vizuale ale copiilor)
    if (m_handle) {
        InvalidateRect(m_handle, NULL, TRUE);
        UpdateWindow(m_handle);
    }
}

void vControl::create(HWND parent) {
    if (!parent) return;

    // Setăm părintele la nivel logic
    // (Presupunând că ai un findControlByHandle sau similar, 
    // poți seta pointerul m_parent aici dacă ai handle-ul părintelui)

    // Determinăm DPI-ul ferestrei părinte pentru scalare corectă
    m_currentDpi = GetDpiForWindow(parent);

    // În mod normal, aici vControl nu apelează CreateWindowEx 
    // pentru că nu știe ce clasă de fereastră să folosească (Button, Edit, etc.)
    // Dar oferă punctul de plecare pentru ierarhie.
   // ConsoleManager::getInstance().log(L"[vControl::create] Base logic executed for: " + str_to_wstr(m_id));
}

vControl* vControl::getChildRecursive(const std::string& id) {
    // 1. Căutăm mai întâi în copiii direcți ai acestui control
    for (auto& pair : m_children) {
        if (pair.first == id) {
            return pair.second.get();
        }
    }

    // 2. Dacă nu l-am găsit, căutăm recursiv în copiii fiecărui sub-control
    for (auto& pair : m_children) {
        vControl* found = pair.second->getChildRecursive(id);
        if (found) {
            return found;
        }
    }

    // 3. Nu a fost găsit nicăieri în ierarhie
    return nullptr;
}


void vControl::setBackgroundColor(COLORREF color) {
    if (m_bgBrush) DeleteObject(m_bgBrush); // Ștergem brush-ul vechi

    m_backgroundColor = color;
    m_bgBrush = CreateSolidBrush(color);
    m_hasCustomBackground = true;

    if (m_handle) {
        InvalidateRect(m_handle, NULL, TRUE); // Forțăm redesenarea controlului
    }
}

void vControl::setTextColor(COLORREF color) {
    m_textColor = color;
    m_hasCustomTextColor = true;

    if (m_handle) {
        InvalidateRect(m_handle, NULL, TRUE);
    }
}


COLORREF vControl::getEffectiveTextColor() const {
    if (m_hasCustomTextColor) return m_textColor;
    if (m_parent) return m_parent->getEffectiveTextColor();
    return GetSysColor(COLOR_WINDOWTEXT); // Fallback la Windows default
}

HBRUSH vControl::getEffectiveBackgroundBrush() const {
    if (m_hasCustomBackground) return m_bgBrush;
    if (m_parent) return m_parent->getEffectiveBackgroundBrush();
    return (HBRUSH)GetStockObject(COLOR_BTNFACE); // Fallback 
}

COLORREF vControl::getEffectiveBackgroundColor() const {
    if (m_hasCustomBackground) return m_backgroundColor;
    if (m_parent) return m_parent->getEffectiveBackgroundColor();
    return GetSysColor(COLOR_BTNFACE); // Fallback
}

HFONT vControl::getEffectiveFont() const {
    // 1. Dacă acest control are propriul font creat, îl returnăm
    if (m_hFont != nullptr) {
        return m_hFont;
    }

    // 2. Dacă nu, întrebăm părintele
    if (m_parent != nullptr) {
        return m_parent->getEffectiveFont();
    }

    // 3. Dacă am ajuns la vârful ierarhiei și nimeni nu are font setat,
    // returnăm fontul sistemului (default)
    return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
}

vControl* vControl::findControlByHandle(HWND hwnd) {
    if (this->m_handle == hwnd) return this;

    for (auto& pair : m_children) {
        vControl* found = pair.second->findControlByHandle(hwnd);
        if (found) return found;
    }
    return nullptr;
}

void vControl::setFont(HFONT hFont) {
    // 1. Stocăm noul font în variabila membră
    m_hFont = hFont;
    m_hasCustomFont = (hFont != nullptr);

    // 2. Dacă controlul are deja un handle (fereastra Win32 există)
    if (m_handle) {
        // Trimitem mesajul standard Win32 către fereastră
        // wParam: handle-ul către font
        // lParam: TRUE pentru a forța redesenarea imediată a controlului
        SendMessage(m_handle, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));

        // Opțional: Forțăm o redesenare completă pentru siguranță
        InvalidateRect(m_handle, NULL, TRUE);
    }
}

bool vControl::validateRecursive() {
    // 1. Validăm controlul curent
    bool thisOk = this->validate();

    // 2. Validăm recursiv toți copiii
    // Deoarece m_children este vector de pair<string, unique_ptr>,
    // folosim child.second pentru a accesa obiectul vControl
    for (auto& childPair : m_children) {
        if (childPair.second) { // Verificăm să nu fie null
            if (!childPair.second->validateRecursive()) {
                thisOk = false; // Dacă un copil e invalid, tot sub-arborele e invalid
            }
        }
    }

    return thisOk;
}





// Funcție ajutătoare internă pentru normalizarea numelui proprietății
static std::wstring normalizePropName(const std::wstring& name) {
    std::wstring lowered = name;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);
    return lowered;
}

bool vControl::setProperty(const std::wstring& name, const vData& value) {
    std::wstring prop = normalizePropName(name);

    if (prop == L"text") {
        this->setText(value.toWString());
        return true;
    }
    else if (prop == L"x") {
        this->setX(static_cast<int>(value.toInt()));
        if (m_handle) MoveWindow(m_handle, m_x, m_y, m_width, m_height, TRUE);
        return true;
    }
    else if (prop == L"y") {
        this->setY(static_cast<int>(value.toInt()));
        if (m_handle) MoveWindow(m_handle, m_x, m_y, m_width, m_height, TRUE);
        return true;
    }
    else if (prop == L"width") {
        this->setWidth(static_cast<int>(value.toInt()));
        if (m_handle) MoveWindow(m_handle, m_x, m_y, m_width, m_height, TRUE);
        return true;
    }
    else if (prop == L"height") {
        this->setHeight(static_cast<int>(value.toInt()));
        if (m_handle) MoveWindow(m_handle, m_x, m_y, m_width, m_height, TRUE);
        return true;
    }
    else if (prop == L"visible") {
        if (value.toBool()) {
            this->show();
        } else {
            this->hide();
        }
        return true;
    }
    else if (prop == L"enabled") {
        this->setEnabled(value.toBool());
        return true;
    }
    else if (prop == L"tooltip") {
        this->setTooltipText(value.toWString());
        return true;
    }
    else if (prop == L"anchor") {
        std::wstring valStr = value.toWString();
        std::transform(valStr.begin(), valStr.end(), valStr.begin(), ::toupper);
        
        Anchor finalAnchor = Anchor::NONE;
        if (valStr == L"CENTER") {
            finalAnchor = Anchor::CENTER;
        } else {
            if (valStr.find(L"LEFT") != std::wstring::npos)     finalAnchor = finalAnchor | Anchor::LEFT;
            if (valStr.find(L"RIGHT") != std::wstring::npos)    finalAnchor = finalAnchor | Anchor::RIGHT;
            if (valStr.find(L"TOP") != std::wstring::npos)     finalAnchor = finalAnchor | Anchor::TOP;
            if (valStr.find(L"BOTTOM") != std::wstring::npos)  finalAnchor = finalAnchor | Anchor::BOTTOM;
            if (valStr.find(L"CENTER_H") != std::wstring::npos) finalAnchor = finalAnchor | Anchor::CENTER_H;
            if (valStr.find(L"CENTER_V") != std::wstring::npos) finalAnchor = finalAnchor | Anchor::CENTER_V;
        }
        
        this->setAnchor(finalAnchor);

        // Notificăm părintele să recalculeze layout-ul
        if (m_parent) {
            m_parent->resize(); // Sau m_parent->applyLayout() în funcție de cum e expus în containere
        }
        return true;
    }
    else if (prop == L"width_mode") {
        std::wstring valStr = value.toWString();
        std::transform(valStr.begin(), valStr.end(), valStr.begin(), ::toupper);

        if (valStr == L"FIXED")        this->setWidthMode(SizeMode::FIXED);
        else if (valStr == L"FILL")    this->setWidthMode(SizeMode::FILL);
        else if (valStr == L"AUTO")    this->setWidthMode(SizeMode::AUTO);
        else if (valStr == L"PERCENT") this->setWidthMode(SizeMode::PERCENT);
        else return false;

        if (m_parent) m_parent->resize();
        return true;
    }
    else if (prop == L"height_mode") {
        std::wstring valStr = value.toWString();
        std::transform(valStr.begin(), valStr.end(), valStr.begin(), ::toupper);

        if (valStr == L"FIXED")        this->setHeightMode(SizeMode::FIXED);
        else if (valStr == L"FILL")    this->setHeightMode(SizeMode::FILL);
        else if (valStr == L"AUTO")    this->setHeightMode(SizeMode::AUTO);
        else if (valStr == L"PERCENT") this->setHeightMode(SizeMode::PERCENT);
        else return false;

        if (m_parent) m_parent->resize();
        return true;
    }
    else if (prop == L"margin") {
        if (value.isArray()) {
            auto* arr = value.rawArray();
            if (arr && arr->size() >= 4) {
                this->setMargins(
                    static_cast<int>((*arr)[0].toInt()),
                    static_cast<int>((*arr)[1].toInt()),
                    static_cast<int>((*arr)[2].toInt()),
                    static_cast<int>((*arr)[3].toInt())
                );
            }
        } else {
            int uniformMargin = static_cast<int>(value.toInt());
            this->setMargins(uniformMargin, uniformMargin, uniformMargin, uniformMargin);
        }

        if (m_parent) m_parent->resize();
        return true;
    }
    else if (prop == L"font_size") {
        this->setFontSize(static_cast<int>(value.toInt()));
		this->scale(m_currentDpi);
        return true;
    }
    else if (prop == L"font_name") {
        this->setFontName(value.toWString());
        return true;
    }
	else if (prop == L"background_color") {
		// Presupunem că primești o valoare de tip 0xRRGGBB (hex sau int)
		COLORREF color = static_cast<COLORREF>(value.toInt());
		this->setBackgroundColor(color);
		return true;
	}

    return false; // Proprietatea nu aparține obiectului vControl de bază
}

vData vControl::getProperty(const std::wstring& name) const {
    std::wstring prop = normalizePropName(name);

    if (prop == L"id")          return vData(str_to_wstr(m_id));
    if (prop == L"text")        return vData(this->getText());
    if (prop == L"x")           return vData(static_cast<long long>(this->getX()));
    if (prop == L"y")           return vData(static_cast<long long>(this->getY()));
    if (prop == L"width")       return vData(static_cast<long long>(this->getWidth()));
    if (prop == L"height")      return vData(static_cast<long long>(this->getHeight()));
    if (prop == L"visible")     return vData(this->isVisible());
    if (prop == L"enabled")     return vData(this->isEnabled());
    if (prop == L"font_size")   return vData(static_cast<long long>(m_baseFontSize));
    if (prop == L"font_name")   return vData(m_fontName);
    
    if (prop == L"anchor") {
        if (anchor == Anchor::CENTER) return vData(L"CENTER");
        std::wstring res = L"";
        if (hasFlag(anchor, Anchor::LEFT))     res += L"LEFT|";
        if (hasFlag(anchor, Anchor::RIGHT))    res += L"RIGHT|";
        if (hasFlag(anchor, Anchor::TOP))      res += L"TOP|";
        if (hasFlag(anchor, Anchor::BOTTOM))   res += L"BOTTOM|";
        if (hasFlag(anchor, Anchor::CENTER_H)) res += L"CENTER_H|";
        if (hasFlag(anchor, Anchor::CENTER_V)) res += L"CENTER_V|";
        if (!res.empty() && res.back() == L'|') res.pop_back();
        return res.empty() ? vData(L"NONE") : vData(res);
    }
    if (prop == L"width_mode") {
        if (widthMode == SizeMode::FIXED)   return vData(L"FIXED");
        if (widthMode == SizeMode::FILL)    return vData(L"FILL");
        if (widthMode == SizeMode::AUTO)    return vData(L"AUTO");
        if (widthMode == SizeMode::PERCENT) return vData(L"PERCENT");
    }
    if (prop == L"height_mode") {
        if (heightMode == SizeMode::FIXED)   return vData(L"FIXED");
        if (heightMode == SizeMode::FILL)    return vData(L"FILL");
        if (heightMode == SizeMode::AUTO)    return vData(L"AUTO");
        if (heightMode == SizeMode::PERCENT) return vData(L"PERCENT");
    }
    if (prop == L"margin") {
        auto marginArray = std::make_shared<std::vector<vData>>();
        marginArray->push_back(vData(static_cast<long long>(marginLeft)));
        marginArray->push_back(vData(static_cast<long long>(marginTop)));
        marginArray->push_back(vData(static_cast<long long>(marginRight)));
        marginArray->push_back(vData(static_cast<long long>(marginBottom)));
        return vData(marginArray);
    }

    // Sistem fallback: verificăm dacă este în atributele generice runtime
    if (this->hasAttribute(name)) {
        return vData(this->getAttribute(name));
    }

    return vData{ std::monostate{} }; // Proprietate inexistentă -> returnează Null în script
}