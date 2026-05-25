#include "vContainer.hpp"
#include "vSpacer.hpp"
#include "vComboBox.hpp"
#include "ConsoleManager.hpp"
#include "ControlIdManager.hpp" // Adăugat pentru a accesa ControlIdManager::getInstance().getId()
#include "stringUtils.hpp"


#include "Layouts/Layouts.hpp" // Include headerele pentru GridLayout, AnchorLayout, FormLayout etc.
#include <algorithm>
#include <windows.h>
#include <shellscalingapi.h>

#include <commctrl.h>

// Dacă folosești Visual Studio, este bine să adaugi și biblioteca pentru linker aici
#pragma comment(lib, "comctl32.lib")

UINT getMonitorDpi(HWND hwnd) {
    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    UINT dpiX = 96, dpiY = 96;

    if (hMonitor) {
        HRESULT hr = GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
        if (SUCCEEDED(hr)) {
            // Poți returna dpiX sau media, dar de obicei sunt la fel.
            return dpiX;
        }
    }
    // Fallback la 96 DPI în caz de eroare.
    return 96;
}


// --- Constructor ---
vContainer::vContainer(
    HINSTANCE hInstance,
    const std::string& id,
    int x, int y, int width, int height, EventDispatcher& dispatcher
) : vControl(hInstance, id, x, y, width, height, dispatcher) { // <- Apelul esențial către constructorul vControl.

  //  ConsoleManager::getInstance().log(L"[vContainer::Constructor] Apelat cu poziție și dimensiune pentru ID: " + std::wstring(id.begin(), id.end()));
}


vContainer::vContainer(HINSTANCE hInstance, const std::string& id, EventDispatcher& dispatcher)
    : vControl(hInstance, id, 0, 0, 0, 0, dispatcher) { // Setează o poziție/dimensiune implicită.

   // ConsoleManager::getInstance().log(L"[vContainer::Constructor] Apelat cu un singur parametru pentru ID: " + std::wstring(id.begin(), id.end()));
}



bool vContainer::handleChildCommand(int controlId, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 1. Căutăm printre copiii direcți
    for (auto& [id, child] : m_children) {
        if (child->getWin32Id() == controlId) {
            // L-am găsit! Este copilul meu direct.
            child->handleMessage(child->getHandle(), msg, wParam, lParam);
            return true;
        }
    }

    // 2. Dacă nu e copil direct, întrebăm sub-containerele (inclusiv vGroupBox)
    for (auto& [id, child] : m_children) {
        vContainer* subContainer = dynamic_cast<vContainer*>(child.get());
        if (subContainer) {
            // Sub-containerul va face aceeași căutare în copiii lui
            if (subContainer->handleChildCommand(controlId, msg, wParam, lParam)) {
                return true;
            }
        }
    }

    return false;
}


bool vContainer::handleChildNotify(LPNMHDR nmhdr, UINT msg, WPARAM wParam, LPARAM lParam) {
    int controlId = static_cast<int>(nmhdr->idFrom);
    HWND hChild = nmhdr->hwndFrom;

    // Pasul 1: Căutăm prin ControlIdManager (ID-ul numeric)
    std::string childInternalId = ControlIdManager::getNameById(controlId);
    if (!childInternalId.empty()) {
        vControl* childPtr = getChild(childInternalId);
        if (childPtr && childPtr->getHandle() == hChild) {
            // Am găsit controlul direct
            childPtr->handleMessage(childPtr->getHandle(), msg, wParam, lParam);
            return true;
        }
    }

    // Pasul 2: Căutare recursivă în sub-containere
    for (auto& pair : m_children) {
        vContainer* subContainer = dynamic_cast<vContainer*>(pair.second.get());
        if (subContainer) {
            if (subContainer->handleChildNotify(nmhdr, msg, wParam, lParam)) {
                return true;
            }
        }
    }

    return false;
}


bool vContainer::routeMessageToChild(int controlId, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 1. Căutăm în copiii direcți
    for (auto& [id, child] : m_children) {
        if (child->getWin32Id() == controlId) {
            child->handleMessage(child->getHandle(), msg, wParam, lParam);
            return true;
        }
    }

    // 2. Căutăm recursiv în sub-containere
    for (auto& [id, child] : m_children) {
        vContainer* subContainer = dynamic_cast<vContainer*>(child.get());
        if (subContainer) {
            if (subContainer->routeMessageToChild(controlId, msg, wParam, lParam)) {
                return true;
            }
        }
    }
    return false;
}

/*
void vContainer::scale(int newDpi) {
    // 1. Scalează panelul/containerul curent
    vControl::scale(newDpi);

    // 2. Propagă la copii
    for (auto& pair : m_children) {
        vControl* child = pair.second.get();
        if (child) {
            //LOG_ERROR(L"PROPAGARE: Trimit scale la " + str_to_wstr(child->getId()));
            child->scale(newDpi);
            //child->scaleFont(newDpi);
        }
         // scale se propaga la copii nu trebuie tratat diferit    
        //vContainer* containerChild = dynamic_cast<vContainer*>(child);
        //if (containerChild) {    }
    }

    // 3. Re-aliniază copiii conform layout-ului
    applyLayout();
}
*/

void vContainer::scale(int newDpi) {
    // 1. Apelăm logica de bază din vControl.
    // Aceasta va scala dimensiunile containerului ȘI va propaga scalarea la copii recursiv.
    vControl::scale(newDpi);

    // 2. RE-ALINIEREA: Aceasta trebuie să se întâmple o SINGURĂ dată, 
    // după ce tot arborele de sub acest container și-a actualizat dimensiunile interne.
    applyLayout();
}

void vContainer::create(HWND parent) {
    // 1. Apelăm logica de bază (setare DPI, etc.)
    vControl::create(parent);

    // 2. Creăm handle-ul containerului (ex: pentru un vPanel)
    // NOTĂ: Logica de CreateWindowEx pentru container trebuie să fie aici
    // m_handle = CreateWindowEx(...);

    if (m_handle) {
        // 3. RECURSIVITATE: Parcurgem copiii din vectorul de perechi
        for (auto& pair : m_children) {
            vControl* child = pair.second.get();
            if (child) {
                // Aici se propagă crearea:
                // Dacă copilul e vLabel -> se apelează vLabel::create
                // Dacă copilul e vPanel -> se apelează vContainer::create (RECURSIUNE)
                child->create(m_handle);

                // Înregistrăm ID-ul Win32 pentru mesaje
                m_controlsByWin32Id[child->getWin32Id()] = child;
            }
        }

        // 4. După ce toți copiii au HWND-uri, așezăm elementele
        applyLayout();
    }
}


// --- Gestionare Mesaje (handleMessage) ---
LRESULT vContainer::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // ConsoleManager::getInstance().log(L"[vContainer::handleMessage] Mesaj: " + std::to_wstring(msg) +
    //     L", HWND: " + std::to_wstring(reinterpret_cast<uintptr_t>(hwnd)) +
    //     L", ID al meu: " + std::wstring(m_id.begin(), m_id.end()));
    

    switch (msg) {
        
        case WM_NOTIFY: {
            // 1. Extragem header-ul notificării din lParam
            LPNMHDR nmhdr = (LPNMHDR)lParam;

            // 2. Pasăm pointerul către handleChildNotify
            // Funcția va căuta controlul și îi va trimite mesajul
            if (handleChildNotify(nmhdr, msg, wParam, lParam)) {
                return 0; // Mesajul a fost procesat de un control copil
            }

            // 3. Dacă handleChildNotify returnează false, înseamnă că mesajul 
            // nu aparține niciunui control gestionat de vContainer sau e o notificare specială
            if (nmhdr->code == DTN_DATETIMECHANGE) {
                LPNMDATETIMECHANGE lpChange = (LPNMDATETIMECHANGE)lParam;
                // Logica suplimentară dacă e nevoie...
            }

            break;
        }

    case WM_COMMAND: {
        int controlID = LOWORD(wParam); // ID-ul Win32 al controlului care trimite comanda
        //LOG_DEBUG(L"[vContainer::handleMessage] Received WM_COMMAND! Source ID: " + std::to_wstring(controlID));// +L":" + str_to_wstr(control->getId()));
        // Deleagă gestionarea comenzii către metoda recursivă handleChildCommand.
        if (handleChildCommand(controlID, msg, wParam, lParam)) {
            return 0; // Mesajul a fost gestionat de un copil sau sub-copil.
        }
        // Dacă handleChildCommand returnează false, înseamnă că niciun copil nu a gestionat mesajul.
        // În acest caz, cădem prin 'break' la apelul clasei de bază.
        break;
    }


                   // TODO: Poți adăuga aici și alte mesaje relevante pentru un container, dacă este necesar.
                   // Exemplu: WM_SIZE (pentru a rearanja copiii la redimensionare), WM_NOTIFY etc.

    case WM_SIZE: {
        applyLayout();
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORLISTBOX:{
        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;

        for (auto& [id, child] : m_children) {

            vControl* raw = child.get();   // <-- pointer brut

            // 1. Este controlul care cere culoare?
            if (raw->getHandle() != hCtrl)
                continue;

            // 2. Spacer debug
            if (raw->isSpacer() && vSpacer::s_debugMode) {
                static HBRUSH hBrush = CreateSolidBrush(RGB(255, 182, 193));
                SetBkColor(hdc, RGB(255, 182, 193));
                return (LRESULT)hBrush;
            }

            // 3. Este ComboBox?
            if (raw->getType() == ControlType::Combobox) {

                vComboBox* combo = static_cast<vComboBox*>(raw);

                SetTextColor(hdc, combo->getTextColor());
                SetBkColor(hdc, combo->getBackgroundColor());

                static HBRUSH hBrush = nullptr;
                if (hBrush) DeleteObject(hBrush);
                hBrush = CreateSolidBrush(combo->getBackgroundColor());

                return (LRESULT)hBrush;
            }
        }

        break;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* lpDIS = (DRAWITEMSTRUCT*)lParam;
        vControl* ctrl = getChildByWin32Id((int)lpDIS->CtlID);
        if (ctrl) {
            // Forțăm apelul handleMessage. 
            // Asigură-te că în vComboBox folosești SendMessageW pentru CB_GETLBTEXT!
            return ctrl->handleMessage(lpDIS->hwndItem, msg, wParam, lParam);
        }
        break;
    }
    case WM_MEASUREITEM: {
        LPMEASUREITEMSTRUCT lpMIS = (LPMEASUREITEMSTRUCT)lParam;
        vControl* ctrl = getChildByWin32Id((int)lpMIS->CtlID);

        if (ctrl) {
            // IMPORTANT: Pentru ComboBox OwnerDraw, itemHeight trebuie să fie 
            // suficient de mare pentru fontul curent.
            int h = ctrl->getHeight();
            lpMIS->itemHeight = (h > 15) ? h : 25;
            return TRUE;
        }
        break;
    }


    default:
        // Pentru orice alt mesaj pe care vContainer nu-l gestionează direct,
        // pasează-l către implementarea clasei de bază (vControl::handleMessage).
        // Acest lucru permite vControl să apeleze DefWindowProc sau să aibă propria logică de bază.
        return vControl::handleMessage(hwnd, msg, wParam, lParam);
    }

    // Dacă un mesaj a fost interceptat în switch (precum WM_COMMAND), dar nu a fost returnat 0
    // de logica `vContainer` (ex: `handleChildCommand` a returnat false),
    // înseamnă că nu a fost gestionat complet. Prin urmare, îl pasăm mai departe la clasa de bază.
    // Această linie este crucială după un 'break' din switch.
    return vControl::handleMessage(hwnd, msg, wParam, lParam);
}



// Funcție ajutătoare pentru normalizarea numelui proprietății în lowercase
static std::wstring toLowerProp(const std::wstring& name) {
    std::wstring lowered = name;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);
    return lowered;
}

// Funcție ajutătoare pentru normalizarea valorii de tip string în uppercase
static std::wstring toUpperVal(const std::wstring& val) {
    std::wstring upper = val;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    return upper;
}

bool vContainer::setProperty(const std::wstring& name, const vData& value) {
    std::wstring prop = toLowerProp(name);

    // Interceptăm proprietatea specifică doar containerelor care acceptă manageri de layout
    if (prop == L"layout") {
        std::wstring layoutType = toUpperVal(value.toWString());

        if (layoutType == L"GRID") {
            // Instanțiem un Grid standard 1x1. Parametrii suplimentari pot fi configurați 
            // ulterior prin atribute sau metode custom, dacă scriptul o cere
            this->setLayoutStrategy(std::make_unique<GridLayout>(1, 1));
            ConsoleManager::getInstance().log(L"📐 [vContainer] Strategia GRID a fost aplicată polimorfic pe: " + str_to_wstr(m_id));
            return true;
        }
        else if (layoutType == L"ANCHOR") {
            this->setLayoutStrategy(std::make_unique<AnchorLayout>());
            ConsoleManager::getInstance().log(L"📐 [vContainer] Strategia ANCHOR a fost aplicată polimorfic pe: " + str_to_wstr(m_id));
            return true;
        }
        else if (layoutType == L"FLOW" || layoutType == L"FORM") {
            this->setLayoutStrategy(std::make_unique<FormLayout>());
            ConsoleManager::getInstance().log(L"📐 [vContainer] Strategia FLOW/FORM a fost aplicată polimorfic pe: " + str_to_wstr(m_id));
            return true;
        }
        else if (layoutType == L"VSTACK") {
            this->setLayoutStrategy(std::make_unique<VerticalStackLayout>());
            ConsoleManager::getInstance().log(L"📐 [vContainer] Strategia VSTACK a fost aplicată polimorfic pe: " + str_to_wstr(m_id));
            return true;
        }
        else if (layoutType == L"HSTACK") {
            this->setLayoutStrategy(std::make_unique<HorizontalPercentStackLayout>());
            ConsoleManager::getInstance().log(L"📐 [vContainer] Strategia HSTACK a fost aplicată polimorfic pe: " + str_to_wstr(m_id));
            return true;
        }
        else if (layoutType == L"NONE" || layoutType == L"NULL") {
            this->setLayoutStrategy(nullptr);
            ConsoleManager::getInstance().log(L"📐 [vContainer] Strategia de layout a fost eliminată pentru: " + str_to_wstr(m_id));
            return true;
        }
        
        return false; // Strategie de layout necunoscută
    }

    // Dacă nu este proprietatea "layout", o pasăm în cascadă către vControl
    // vControl se va ocupa de text, x, y, width, height, margin, anchor etc.
    return vControl::setProperty(name, value);
}

vData vContainer::getProperty(const std::wstring& name) const {
    std::wstring prop = toLowerProp(name);

    // Citirea tipului de layout activat în container
    if (prop == L"layout") {
        if (!m_layoutStrategy) {
            return vData(L"NONE");
        }
        
        // Dacă ai nevoie să întorci tipul exact ca string, putem face verificări de tip RTTI (dynamic_cast) 
        // pe m_layoutStrategy.get(), sau poți lăsa o identificare generică.
        if (dynamic_cast<GridLayout*>(m_layoutStrategy.get())) return vData(L"GRID");
        if (dynamic_cast<AnchorLayout*>(m_layoutStrategy.get())) return vData(L"ANCHOR");
        if (dynamic_cast<FormLayout*>(m_layoutStrategy.get())) return vData(L"FLOW");
        if (dynamic_cast<VerticalStackLayout*>(m_layoutStrategy.get())) return vData(L"VSTACK");
        if (dynamic_cast<HorizontalPercentStackLayout*>(m_layoutStrategy.get())) return vData(L"HSTACK");
        
        return vData(L"CUSTOM");
    }

    // Dacă nu e "layout", lăsăm vControl să rezolve citirea proprietății
    return vControl::getProperty(name);
}