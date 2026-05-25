#pragma once

#include "vWindow.hpp"
#include "vLabel.hpp"
#include "vButton.hpp"
#include "vPanel.hpp"
#include "vApp.hpp"
#include "Layouts/Layouts.hpp"

enum class MessageType {
    Info,
    Warning,
    Error,
    Question
};

enum class MessageButtons {
    Ok,
    OkCancel,
    YesNo
};

class vMessageDialog : public vWindow {
private:
    vLabel* m_messageLabel = nullptr;
    vPanel* m_buttonPanel = nullptr;
    std::string m_result = "cancel"; // "ok", "yes", "no", "cancel"
    bool m_modalActive = false;

public:
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override {
        if (msg == WM_CLOSE) {
            // Utilizatorul a apăsat X
            m_result = "cancel";
            setModalActive(false); // DEBLOCHEAZĂ bucla modală din show()
            // Nu returnăm 0 aici dacă vWindow::handleMessage face DestroyWindow, 
            // dar e mai sigur să lăsăm clasa de bază să curețe restul.
        }
        return vWindow::handleMessage(hwnd, msg, wParam, lParam);
    }
    /*
    vMessageDialog(HINSTANCE hInstance, EventDispatcher& dispatcher,
        const std::wstring& title, const std::wstring& message,
        MessageType type = MessageType::Info,
        MessageButtons buttons = MessageButtons::Ok)
        : vWindow(hInstance, "msgDlg", WindowType::DialogWindow, false, dispatcher)
    {
        // 1. Root Layout - Stack Vertical
        this->setLayoutStrategy(std::make_unique<VerticalStackLayout>());

        // 2. Creare fereastră (dimensiune adaptabilă)
        //this->create(L"VMessageDlgClass", title, WS_POPUP | WS_CAPTION | WS_SYSMENU,
        //    0, 0, 430, 200, vApp::getAppInstance()->getMainWindow(), nullptr);
        this->create(L"VMessageDlgClass", title, WS_POPUP | WS_CAPTION | WS_SYSMENU,
            0, 0, 430, 200, GetActiveWindow(), nullptr);

        if (getHandle()) {
            // 3. Panel conținut (Text)
            auto contentPanel = std::make_unique<vPanel>(m_hInstance, "msgContent", 0, 0, 400, 100, getEventDispatcher());
            contentPanel->setHeightMode(SizeMode::FILL);
            contentPanel->setMargins(10, 10, 10, 10);
            vPanel* pMainPanel = contentPanel.get();
            this->addChild("content", std::move(contentPanel));
            

            auto label = std::make_unique<vLabel>(m_hInstance, "lblMsg", message, 0, 10, 360, 60, getEventDispatcher());
            m_messageLabel = label.get();
            pMainPanel->addChild("lblMsg", std::move(label));

          

            // 4. Panel butoane (Centrat folosind metoda Spacer-elor)
            auto btnPanel = std::make_unique<vPanel>(m_hInstance, "msgButtons", 0, 0, 400, 50, getEventDispatcher());
            btnPanel->setHeightMode(SizeMode::FIXED);
            btnPanel->setLayoutStrategy(std::make_unique<FlexStackLayout>());
            btnPanel->setMargins(10, 10, 10, 10);
            m_buttonPanel = btnPanel.get();
            this->addChild("buttons", std::move(btnPanel));

            setupButtons(buttons);
            

            this->applyLayout();
            this->centerWindow();
        }
    }
    */

    vMessageDialog(HINSTANCE hInstance, EventDispatcher& dispatcher,
        const std::wstring& title, const std::wstring& message,
        MessageType type = MessageType::Info,
        MessageButtons buttons = MessageButtons::Ok)
        : vWindow(hInstance, "msgDlg", WindowType::DialogWindow, false, dispatcher)
    {
        this->setLayoutStrategy(std::make_unique<VerticalStackLayout>());

        // --- CALCUL DIMENSIUNE TEXT ---
        // Calculăm aproximativ înălțimea necesară (sau folosim o metodă WinAPI DrawText cu DT_CALCRECT)
        int lineCount = std::count(message.begin(), message.end(), L'\n') + 1;
        int estimatedHeight = 60 + (lineCount * 20); // 20px per rând extra
        if (message.length() > 100) estimatedHeight += 40; // Extra pentru wrap-around

        int winWidth = 450;
        int winHeight = 100 + estimatedHeight; // Text + Butoane + Margini

        // Creăm fereastra cu noua înălțime calculată
        this->create(L"VMessageDlgClass", title, WS_POPUP | WS_CAPTION | WS_SYSMENU,
            0, 0, winWidth, winHeight, GetActiveWindow(), nullptr);

        if (getHandle()) {
            // 3. Panel conținut
            auto contentPanel = std::make_unique<vPanel>(m_hInstance, "msgContent", 0, 0, winWidth - 30, estimatedHeight, getEventDispatcher());
            contentPanel->setHeightMode(SizeMode::FILL); // Ocupă tot spațiul până la butoane
            contentPanel->setMargins(15, 15, 15, 10);
            vPanel* pMainPanel = contentPanel.get();
            this->addChild("content", std::move(contentPanel));

            // Label-ul trebuie să aibă și el înălțime adaptabilă
            auto label = std::make_unique<vLabel>(m_hInstance, "lblMsg", message, 0, 0, winWidth - 60, estimatedHeight, getEventDispatcher());
            m_messageLabel = label.get();
            pMainPanel->addChild("lblMsg", std::move(label));

            // 4. Panel butoane
            auto btnPanel = std::make_unique<vPanel>(m_hInstance, "msgButtons", 0, 0, winWidth - 30, 50, getEventDispatcher());
            btnPanel->setHeightMode(SizeMode::FIXED);
            btnPanel->setLayoutStrategy(std::make_unique<FlexStackLayout>());
            btnPanel->setMargins(10, 10, 10, 10);
            m_buttonPanel = btnPanel.get();
            this->addChild("buttons", std::move(btnPanel));

            setupButtons(buttons);

            this->applyLayout();
            this->centerWindow();
        }
    }

    // Shortcut-uri în interiorul clasei
    static void Info(const std::wstring& message, const std::wstring& title = L"Informație") {
        show(title, message, MessageButtons::Ok);
    }

    static void Warning(const std::wstring& message, const std::wstring& title = L"Atenție") {
        show(title, message, MessageButtons::Ok);
    }

    static void Error(const std::wstring& message, const std::wstring& title = L"Eroare") {
        show(title, message, MessageButtons::Ok);
    }

    static bool Confirm(const std::wstring& message, const std::wstring& title = L"Confirmare") {
        return show(title, message, MessageButtons::YesNo) == "yes";
    }

    std::string getResult() { return m_result; }
    /*
    static std::string show(
        //HINSTANCE hInstance,
        const std::wstring& title,
        const std::wstring& message,
        MessageButtons buttons = MessageButtons::Ok)
    {
        EventDispatcher& disp =  vApp::getAppInstance()->getEventDispatcher();
        HINSTANCE hInst = vApp::getAppInstance()->getInstance();
        //vMessageDialog dlg(hInstance, dispatcher, title, message, MessageType::Question, buttons);
        vMessageDialog dlg(hInst, disp, title, message, MessageType::Question, buttons);

        HWND hParent = GetActiveWindow();
        if (hParent) EnableWindow(hParent, FALSE); // Blocăm input-ul la părinte

        // CALIFICARE EXPLICITĂ: Spunem compilatorului să caute 'show' în clasa de bază
        dlg.vWindow::show();

        dlg.setModalActive(true);

        MSG msg;
        while (dlg.isModalActive() && GetMessage(&msg, nullptr, 0, 0)) {
            // Procesăm mesajele pentru dialog și copiii săi
            if (!IsDialogMessage(dlg.getHandle(), &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            // Gestionăm închiderea forțată a aplicației
            if (msg.message == WM_QUIT) {
                PostQuitMessage((int)msg.wParam);
                break;
            }
        }

        if (hParent) {
            EnableWindow(hParent, TRUE);
            SetForegroundWindow(hParent);
            SetFocus(hParent); // Redăm focusul către fereastra care a apelat
        }

        disp.removeHandlers("msgdlg_btnOk_" + std::to_string(reinterpret_cast<size_t>(&dlg)));
        disp.removeHandlers("msgdlg_btnYes_" + std::to_string(reinterpret_cast<size_t>(&dlg)));
        disp.removeHandlers("msgdlg_btnNo_" + std::to_string(reinterpret_cast<size_t>(&dlg)));
        disp.removeHandlers("msgdlg_btnCancel_" + std::to_string(reinterpret_cast<size_t>(&dlg)));

        return dlg.getResult();
    }
    */
    /*
    static std::string show(
        const std::wstring& title,
        const std::wstring& message,
        MessageButtons buttons = MessageButtons::Ok)
    {
        EventDispatcher& disp = vApp::getAppInstance()->getEventDispatcher();
        HINSTANCE hInst = vApp::getAppInstance()->getInstance();

        // 1. Detectăm fereastra care are focusul ACUM (fereastra de editare, probabil)
        HWND hParent = GetActiveWindow();
        if (hParent == NULL) hParent = vApp::getAppInstance()->getMainWindow();

        // 2. Creăm dialogul cu hParent ca proprietar real, nu hardcodat pe MainWindow
        vMessageDialog dlg(hInst, disp, title, message, MessageType::Question, buttons);

        // Dacă vrei să schimbi părintele după crearea în constructor, 
        // trebuie să te asiguri că funcția create() din vMessageDialog folosește hParent.
        // O soluție rapidă este să modifici constructorul să primească hParent:
        // SetWindowLongPtr(dlg.getHandle(), GWLP_HWNDPARENT, (LONG_PTR)hParent);

        if (hParent) EnableWindow(hParent, FALSE);

        dlg.vWindow::show();
        dlg.setModalActive(true);

        MSG msg;
        while (dlg.isModalActive() && GetMessage(&msg, nullptr, 0, 0)) {
            if (!IsDialogMessage(dlg.getHandle(), &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if (msg.message == WM_QUIT) {
                PostQuitMessage((int)msg.wParam);
                break;
            }
        }

        // 3. ORDINEA CORECTĂ DE REACTIVARE
        if (hParent) {
            EnableWindow(hParent, TRUE);    // Mai întâi deblocăm
            SetActiveWindow(hParent);       // O setăm ca activă
            SetForegroundWindow(hParent);   // O forțăm în față
            SetFocus(hParent);              // Îi dăm focusul tastaturii
        }

        // Curățare handlere...
        return dlg.getResult();
    }
    */

static std::string show(const std::wstring& title, const std::wstring& message, MessageButtons buttons = MessageButtons::Ok) {
    EventDispatcher& disp = vApp::getAppInstance()->getEventDispatcher();
    HINSTANCE hInst = vApp::getAppInstance()->getInstance();

    HWND hParent = GetActiveWindow();
    if (hParent == NULL) hParent = vApp::getAppInstance()->getMainWindow();

    vMessageDialog dlg(hInst, disp, title, message, MessageType::Question, buttons);

    // Blocăm părintele
    if (hParent) EnableWindow(hParent, FALSE);

    dlg.vWindow::show();
    dlg.setModalActive(true);

    MSG msg;
    // CRITICAL: Verificăm și IsWindow pentru a nu rămâne blocați dacă hWnd devine invalid
    while (dlg.isModalActive() && IsWindow(dlg.getHandle())) {
        if (GetMessage(&msg, nullptr, 0, 0)) {
            if (!IsDialogMessage(dlg.getHandle(), &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if (msg.message == WM_QUIT) {
                PostQuitMessage((int)msg.wParam);
                break;
            }
        }
        else break;
    }

    // --- REPARAȚIA PENTRU FOCUS ---
    if (hParent && IsWindow(hParent)) {
        EnableWindow(hParent, TRUE);   // 1. Deblocăm

        // Ordinea aceasta previne trimiterea în spate:
        SetForegroundWindow(hParent);  // 2. Aducem în față
        SetActiveWindow(hParent);      // 3. Activăm
        SetFocus(hParent);             // 4. Focus tastatură
    }

    // Curățăm handler-ele folosind adresa obiectului dlg (unicitate)
    std::string addr = std::to_string(reinterpret_cast<size_t>(&dlg));
    disp.removeHandlers("msgdlg_btnOk_" + addr);
    disp.removeHandlers("msgdlg_btnYes_" + addr);
    disp.removeHandlers("msgdlg_btnNo_" + addr);
    disp.removeHandlers("msgdlg_btnCancel_" + addr);

    return dlg.getResult();
}

    bool isModalActive() const { return m_modalActive; }
    void setModalActive(bool active) { m_modalActive = active; }

private:
   void setupButtons(MessageButtons buttons) {
        // Spacer stânga pentru centrare
        //auto spacerL = std::make_unique<vPanel>(m_hInstance, "spL", 0, 0, 0, 0, getEventDispatcher());
        //spacerL->setWidthMode(SizeMode::FILL);
        //m_buttonPanel->addChild("spL", std::move(spacerL));
        auto spacerL = std::make_unique<vPanel>(m_hInstance, "spL", 0, 0, 0, 0, getEventDispatcher());
        spacerL->setWidthMode(SizeMode::FILL);
        m_buttonPanel->addChild("spL", std::move(spacerL));

        if (buttons == MessageButtons::Ok || buttons == MessageButtons::OkCancel) {
            addButton("btnOk", L"&OK", "ok");
        }
        if (buttons == MessageButtons::YesNo) {
            addButton("btnYes", L"Da", "yes");
            addButton("btnNo", L"Nu", "no");
        }
        if (buttons == MessageButtons::OkCancel) {
            addButton("btnCancel", L"Renunță", "cancel");
        }

        // Spacer dreapta pentru centrare
        //auto spacerR = std::make_unique<vPanel>(m_hInstance, "spR", 0, 0, 0, 0, getEventDispatcher());
        //spacerR->setWidthMode(SizeMode::FILL);
        //m_buttonPanel->addChild("spR", std::move(spacerR));
        auto spacerR = std::make_unique<vPanel>(m_hInstance, "spR", 0, 0, 0, 0, getEventDispatcher());
        spacerR->setWidthMode(SizeMode::FILL);
        m_buttonPanel->addChild("spR", std::move(spacerR));
    }

    void addButton(const std::string& id, const std::wstring& text, const std::string& resultValue) {
        // Generăm un ID care include adresa dialogului (garantat unic în memorie)
        std::string uniqueId = "msgdlg_" + id + "_" + std::to_string(reinterpret_cast<size_t>(this));

        auto btn = std::make_unique<vButton>(m_hInstance, uniqueId, text, 0, 0, 90, 30, getEventDispatcher());

        getEventDispatcher().registerHandler("click", uniqueId, [this, resultValue]() {
            m_result = resultValue;
            this->setModalActive(false); // Aceasta va opri bucla while din show()
            this->hide();
            });

        m_buttonPanel->addChild(uniqueId, std::move(btn));
    }
};