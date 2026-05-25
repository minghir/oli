#include "vPanel.hpp"
#include "vGrid.hpp"
#include "vEdit.hpp"
#include "stringUtils.hpp"
//#define _WIN32_IE 0x0500 // sau mai mare

#include <windowsx.h>
#include <commctrl.h>
#include <algorithm>

// Initialize the static member (outside of any function).
ATOM vPanel::s_panelClassAtom = 0;

// --- Constructors ---
// Implementation of the constructor with position and size parameters.
vPanel::vPanel(
    HINSTANCE hInstance,
    const std::string& id,
    int x, int y, int width, int height, EventDispatcher& dispatcher
) : vContainer(hInstance, id, x, y, width, height, dispatcher), // Correctly call the vContainer constructor.
//m_hInstance(hInstance),
//m_backgroundColor(RGB(240, 240, 240)),
m_isPressed(false)
{
   // ConsoleManager::getInstance().log(L"[vPanel::Constructor] Called with dimensions for ID: " + str_to_wstr(id));
    // Ensure the class is registered only once.
    if (s_panelClassAtom == 0) {
        s_panelClassAtom = registerPanelClass(hInstance);
    }
}
/*
// Implementation of the constructor with only an ID.
vPanel::vPanel(HINSTANCE hInstance, const std::string& id, EventDispatcher& dispatcher)
    : vContainer(id, 0, 0, 800, 600, dispatcher), // Call vContainer with default values.
    m_hInstance(hInstance),
    m_backgroundColor(RGB(240, 240, 240)),
    m_isPressed(false)
{
    ConsoleManager::getInstance().log(L"[vPanel::Constructor] Called with a single parameter for ID: " + str_to_wstr(id));
    if (s_panelClassAtom == 0) {
        s_panelClassAtom = registerPanelClass(hInstance);
    }
}
*/
// --- The 'create' method (Crucial fix!) ---
// This is the implementation of the pure virtual method from vControl.
// It uses the dimensions stored in the base class to create the WinAPI window.
void vPanel::create(HWND parent) {
   // ConsoleManager::getInstance().log(L"[vPanel::create] Creating panel with ID: " + str_to_wstr(m_id) + L", in parent HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(parent)));

    if (!parent) {
        return; // Return void as the method signature is void
    }

    const ATOM panelClassAtom = s_panelClassAtom;
    if (panelClassAtom == 0) {
             return;
    }

    const int panelId = getWin32Id();

   UINT parentDpi = GetDpiForWindow(parent);
    // Apelează metoda de scalare cu DPI-ul obținut
   scale(parentDpi);
   
   DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
   if (m_scrollBarOn) {
       dwStyle |= WS_VSCROLL;
   }

    m_handle = CreateWindowExW(
        WS_EX_CONTROLPARENT,
        //MAKEINTATOM(panelClassAtom),
        reinterpret_cast<LPCWSTR>(MAKEINTATOM(panelClassAtom)),
        nullptr,
        dwStyle,
        //m_x, m_y, m_width, m_height,  // <-- THIS IS THE KEY FIX!
        getX(), getY(), getWidth()+600, getHeight(),
        parent,
        (HMENU)(uintptr_t)panelId,
        m_hInstance,
        this
    );

    if (!m_handle) {
        LOG_ERROR(L"vPanel creation failed for ID: " + str_to_wstr(m_id));
        return;
    }
    else {
        
        // Dacă avem un font setat (din XML/applyCommonAttributes), aplică-l pe HWND
        HFONT hf = getEffectiveFont();
        if (hf) {
            SendMessage(m_handle, WM_SETFONT, (WPARAM)hf, TRUE);
        }
        

        GetClientRect(m_handle, &m_originalClientRect);
        SendMessage(m_handle, WM_SIZE, 0,
            MAKELPARAM(m_originalClientRect.right - m_originalClientRect.left,
                m_originalClientRect.bottom - m_originalClientRect.top));
    }
}


// --- Set background color ---
/*
void vPanel::setBackgroundColor(COLORREF color) {
    m_backgroundColor = color;
    if (m_handle) {
        InvalidateRect(m_handle, nullptr, TRUE);
    }
}
*/

// --- 'onClick' method (overridden from vControl) ---
void vPanel::onClick() {
   // ConsoleManager::getInstance().log(L"[vPanel::onClick] Base onClick method called for ID: " + str_to_wstr(m_id));
    getEventDispatcher().dispatch("click");
}

// --- 'onMouseClick' method with coordinates ---
void vPanel::onMouseClick(int x, int y) {
   // ConsoleManager::getInstance().log(L"[vPanel::onMouseClick] Panel was clicked at (" + std::to_wstring(x) + L", " + std::to_wstring(y) + L") for ID: " + str_to_wstr(m_id));
    onClick();
}

// --- Window class registration method ---
ATOM vPanel::registerPanelClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc = { 0 }; // Forțează W
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = vControl::StaticWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"VPanelClass"; // Rămâne L pentru că e wide
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    // Folosește un background null sau sistem, dar asigură-te că e consistent
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;

    ATOM atom = RegisterClassExW(&wc); // Forțează W
    if (atom == 0) {
        ConsoleManager::getInstance().log(L"[ERROR] Failed to register VPanelClass. Error code: " + std::to_wstring(GetLastError()));
    }
    else {
       // ConsoleManager::getInstance().log(L"[vPanel] Window class 'VPanelClass' successfully registered. Atom: " + std::to_wstring(atom));
    }
    return atom;
}


LRESULT vPanel::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Selective logging to prevent spam
    if (msg != WM_SETCURSOR && msg != WM_NCHITTEST && msg != WM_MOUSEMOVE && msg != WM_TIMER) {
       // ConsoleManager::getInstance().log(L"[vPanel ("+ str_to_wstr( getId() ) + L")::handleMessage] Message received: " + std::to_wstring(msg) + L", HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(hwnd)) + L", Panel ID: " + str_to_wstr(m_id));
    }

    //LOG_DEBUG(L"[vPanel (" + str_to_wstr(getId()) + L")::handleMessage] Message received: " + std::to_wstring(msg) + L", HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(hwnd)) + L", Panel ID: " + str_to_wstr(m_id));
    //int controlIDa = LOWORD(wParam);
   // int notificationCode = HIWORD(wParam);

    //LOG_DEBUG(L"[vPanel::handleMessage] Received WM_COMMAND! Source ID: " + std::to_wstring(controlIDa));// +L":" + str_to_wstr(control->getId()));

   
    
    // Handle WM_NOTIFY messages
    if (msg == WM_NOTIFY) {
        LPNMHDR lpnmhdr = reinterpret_cast<LPNMHDR>(lParam);

        for (const auto& pair : m_controlsByWin32Id) {
            vControl* control = pair.second;

          //  LOG_DEBUG(L"[vPanel::handleMessage] Received WM_COMMAND! Source ID: " + std::to_wstring(control->getWin32Id()) + L":" + str_to_wstr(control->getId()) );

            // Verificăm dacă notificarea vine DIRECT de la control (ex: NM_CLICK pe Grid)
            bool isDirectNotify = (control->getHandle() == lpnmhdr->hwndFrom);

            // Verificăm dacă notificarea vine de la HEADER-ul unui control de tip Grid
            bool isHeaderNotify = false;
            vGrid* grid = dynamic_cast<vGrid*>(control);
            if (grid) {
                HWND hHeader = ListView_GetHeader(grid->getHandle());
                if (lpnmhdr->hwndFrom == hHeader) {
                    isHeaderNotify = true;
                }
            }

            if (isDirectNotify || isHeaderNotify) {
                // ConsoleManager::getInstance().log(L"[vPanel::handleMessage] Redirecting WM_NOTIFY from " + str_to_wstr(control->getId()));

                // Trimitem mesajul către vGrid/vControl
                LRESULT res = control->handleMessage(hwnd, msg, wParam, lParam);

                // CRITIC: Pentru blocarea coloanelor, dacă vGrid a returnat TRUE, 
                // trebuie să returnăm și noi TRUE imediat fără să mai trecem prin DefWindowProc
                if (res == TRUE && (lpnmhdr->code == HDN_BEGINTRACKW || lpnmhdr->code == HDN_BEGINTRACKA ||
                    lpnmhdr->code == HDN_ITEMCHANGINGW || lpnmhdr->code == HDN_ITEMCHANGINGA ||
                    lpnmhdr->code == HDN_DIVIDERDBLCLICKW || lpnmhdr->code == HDN_DIVIDERDBLCLICKA)) {
                    return TRUE;
                }

                return res;
            }
        }
    }


    switch (msg) {
    case WM_VSCROLL: {
        SCROLLINFO si = { sizeof(si), SIF_ALL };
        GetScrollInfo(hwnd, SB_VERT, &si);
        int oldPos = si.nPos;

        switch (LOWORD(wParam)) {
        case SB_LINEUP:   si.nPos -= 20; break;
        case SB_LINEDOWN: si.nPos += 20; break;
        case SB_PAGEUP:   si.nPos -= si.nPage; break;
        case SB_PAGEDOWN: si.nPos += si.nPage; break;
        case SB_THUMBTRACK: si.nPos = si.nTrackPos; break;
        }

        // Validăm limitele
        if (si.nPos < 0) si.nPos = 0;
        if (si.nPos > (si.nMax - (int)si.nPage)) si.nPos = si.nMax - si.nPage;

        if (si.nPos != oldPos) {
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            // Deplasăm toți copiii panoului pe axa Y
            ScrollWindowEx(hwnd, 0, oldPos - si.nPos, NULL, NULL, NULL, NULL, SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
        }
        return 0;
    }

                   // Opțional: Suport pentru rotița mouse-ului
    case WM_MOUSEWHEEL: {
        int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        SendMessage(hwnd, WM_VSCROLL, zDelta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
        return 0;
    }
                      /*
    case WM_ERASEBKGND: {
        return 1;
        // Handle WM_ERASEBKGND to avoid flicker and prepare for background drawing.
        // This is crucial for panels to prevent child controls from flickering.
        HDC hdc = (HDC)wParam;
        RECT rect;
        GetClientRect(hwnd, &rect);
        HBRUSH hBrush = CreateSolidBrush(m_backgroundColor);
        FillRect(hdc, &rect, hBrush);
        DeleteObject(hBrush);
        return 1; // Return 1 to indicate that we handled the erase background message.
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Fill the background with the correct color only in the invalidated area.
        HBRUSH hBrush = CreateSolidBrush(m_backgroundColor);
        FillRect(hdc, &ps.rcPaint, hBrush);
        DeleteObject(hBrush);

        // If the panel is "pressed", draw a border.
        if (m_isPressed) {
            HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

            Rectangle(hdc, 0, 0, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top);

            SelectObject(hdc, hOldBrush);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    */
    case WM_COMMAND: {
        int controlID = LOWORD(wParam);
        int notificationCode = HIWORD(wParam);

        vControl* child = getChildByWin32Id(controlID);

        if (child) {
            // Logăm recepția mesajului de la un copil cunoscut
          //  LOG_DEBUG(L"[vPanel::WM_COMMAND] Sursă: " + str_to_wstr(child->getId()) +
          //      L" | ID Win32: " + std::to_wstring(controlID) +
          //      L" | Notificare: 0x" + std::to_wstring(notificationCode));

            // Verificăm dacă sursa este un vEdit
            vEdit* editCtrl = dynamic_cast<vEdit*>(child);

            if (editCtrl) {
                if (notificationCode == EN_KILLFOCUS) {
                    LOG_DEBUG(L"   [FILTRU] Blocat EN_KILLFOCUS pentru vEdit '" + str_to_wstr(child->getId()) + L"' (deja gestionat de Subclass)");
                    return 0;
                }
                if (notificationCode == EN_SETFOCUS) {
                    LOG_DEBUG(L"   [FILTRU] Blocat EN_SETFOCUS pentru vEdit '" + str_to_wstr(child->getId()) + L"'");
                    return 0;
                }
            }

            // Dacă trece de filtru, logăm redirecționarea
            //LOG_DEBUG(L"   [OK] Redirecționare WM_COMMAND către handleMessage al controlului: " + str_to_wstr(child->getId()));
            return child->handleMessage(hwnd, msg, wParam, lParam);
        }
        else {
            // Logăm dacă primim comenzi de la ID-uri pe care panoul nu le are în map
            // (util pentru debug dacă apar butoane sau meniuri orfane)
            LOG_WARNING(L"[vPanel::WM_COMMAND] Primit ID necunoscut: " + std::to_wstring(controlID));
        }
        break;
    }

    case WM_LBUTTONDOWN: {
        // ... (existing code for LBUTTONDOWN) ...
        m_isPressed = true;
        SetCapture(hwnd);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }
    case WM_LBUTTONUP: {
        // ... (existing code for LBUTTONUP) ...
        if (m_isPressed) {
            m_isPressed = false;
            ReleaseCapture();
            InvalidateRect(hwnd, nullptr, TRUE);

            // Re-map coordinates to the parent container's context if needed.
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);

            RECT clientRect;
            GetClientRect(hwnd, &clientRect);

            if (xPos >= clientRect.left && xPos <= clientRect.right && yPos >= clientRect.top && yPos <= clientRect.bottom) {
                onMouseClick(xPos, yPos);
            }
        }
        return 0;
    }
    case WM_SIZE: {
        // Las containerul să facă layout
        LRESULT r = vContainer::handleMessage(hwnd, msg, wParam, lParam);

        // IMPORTANT: actualizăm originalClientRect după primul layout
        RECT rc;
        GetClientRect(hwnd, &rc);
        m_originalClientRect = rc;

        return r;

    }
    case WM_DRAWITEM:
        return vContainer::handleMessage(hwnd, msg, wParam, lParam);
    default:
        // Pass unhandled messages to the default window procedure.
        return vContainer::handleMessage(hwnd, msg, wParam, lParam);
    }


    return vContainer::handleMessage(hwnd, msg, wParam, lParam);
   // return 0;
}






// Funcție ajutătoare locală pentru transformarea în lowercase
static std::wstring toLowerPropPanel(const std::wstring& name) {
    std::wstring lowered = name;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);
    return lowered;
}

bool vPanel::setProperty(const std::wstring& name, const vData& value) {
    std::wstring prop = toLowerPropPanel(name);

    // Proprietate specifică doar pentru vPanel
    if (prop == L"scroll_bar" || prop == L"scrollbar") {
        bool scrollOn = value.toBool();
        this->setScrollBarOn(scrollOn);
        
        // AICI: Dacă ai nevoie să modifici dinamic stilurile ferestrei Win32 (WS_VSCROLL)
        // în funcție de starea scrollBarOn, o poți face aici, urmată de un refresh:
        // if (m_handle) {
        //     LONG_PTR style = GetWindowLongPtr(m_handle, GWL_STYLE);
        //     if (scrollOn) style |= WS_VSCROLL; else style &= ~WS_VSCROLL;
        //     SetWindowLongPtr(m_handle, GWL_STYLE, style);
        //     SetWindowPos(m_handle, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        // }
        
        ConsoleManager::getInstance().log(L"📦 [vPanel] S-a modificat starea scrollbar pe panoul: " + str_to_wstr(m_id));
        return true;
    }

    // Dacă nu este proprietatea specifică panoului, o pasăm în sus către vContainer
    // vContainer va verifica dacă e un "layout", iar dacă nu, o va trimite la vControl
    return vContainer::setProperty(name, value);
}

vData vPanel::getProperty(const std::wstring& name) const {
    std::wstring prop = toLowerPropPanel(name);

    if (prop == L"scroll_bar" || prop == L"scrollbar") {
        return vData(m_scrollBarOn);
    }

    // Fallback către lanțul de moștenire (vContainer -> vControl)
    return vContainer::getProperty(name);
}