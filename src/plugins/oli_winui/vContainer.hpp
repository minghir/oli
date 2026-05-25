#ifndef VCONTAINER_HPP
#define VCONTAINER_HPP

#pragma once

#include "vControl.hpp"      // vContainer moștenește de la vControl, deci este necesar
#include "ConsoleManager.hpp" // Pentru logare
#include "ILayoutStrategy.hpp"

// Nota: m_children și metodele asociate (addChild, getChild, removeChild)
// sunt deja definite în vControl. Astfel, vContainer, vWindow și vPanel
// (dacă și ele moștenesc vControl) vor reutiliza aceste funcționalități.

// Clasa vContainer reprezintă un control WinAPI care poate conține alte controale.
// Ea extinde funcționalitățile de bază ale vControl pentru a oferi dispecerizarea
// mesajelor (precum WM_COMMAND) către copiii săi.
class vContainer : public vControl {
protected:
        std::unique_ptr<ILayoutStrategy> m_layoutStrategy; // Strategia de layout

public:
    // Constructor. Inițializează vContainer cu un ID specific.
    explicit vContainer(HINSTANCE hInstance, const std::string& id, EventDispatcher& dispatcher);

    explicit vContainer(
        HINSTANCE hInstance,
        const std::string& id,
        int x, int y, int width, int height,
        EventDispatcher& dispatcher
    );

    // Nu este necesar un destructor explicit, deoarece destructorul vControl
    // și unique_ptr-urile din m_children gestionează curățenia.
    virtual ~vContainer() = default;

    // Gestionează recursiv mesajele WM_COMMAND primite de la controalele copil.
    // Această metodă caută controlul sursă pe baza ID-ului său Win32
    // și declanșează evenimente specifice (ex: "click" pentru butoane).
    // controlId: ID-ul Win32 al controlului care a trimis mesajul.
    // msg: Mesajul WinAPI (ar trebui să fie WM_COMMAND aici).
    // wParam, lParam: Parametrii mesajului WM_COMMAND.
    // Returnează true dacă mesajul a fost gestionat de un copil, false altfel.
    virtual bool handleChildCommand(int controlId, UINT msg, WPARAM wParam, LPARAM lParam);
    virtual bool handleChildNotify(LPNMHDR nmhdr, UINT msg, WPARAM wParam, LPARAM lParam);
    // Notă: Poți adăuga și o metodă similară pentru WM_NOTIFY, dacă este necesar:
    // virtual bool handleChildNotify(int controlId, LPNMHDR lpnmhdr);

    // Suprascrie metoda handleMessage din vControl pentru a intercepta și procesa
    // mesaje specifice containerului, cum ar fi WM_COMMAND de la copii.
    // HWND, UINT, WPARAM, LPARAM: Parametrii standard ai mesajului WinAPI.
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    bool routeMessageToChild(int controlId, UINT msg, WPARAM wParam, LPARAM lParam);

    void create(HWND parent) override;

    /**
     * @brief Setează strategia de layout.
     * @param strategy Un pointer la noua strategie de layout.
     */
    void setLayoutStrategy(std::unique_ptr<ILayoutStrategy> strategy) {
        m_layoutStrategy = std::move(strategy);

        applyLayout();
    }


    /**
     * @brief Aplică layout-ul curent controalelor copil.
     * Ar trebui apelată după ce dimensiunea containerului se schimbă (WM_SIZE)
     * sau după adăugarea/îndepărtarea unui copil.
     */
    virtual void applyLayout() {
        if (m_layoutStrategy) {
            m_layoutStrategy->applyLayout(*this);
        }

        for (auto& entry : m_children) {
            vContainer* childCont = dynamic_cast<vContainer*>(entry.second.get());
            if (childCont && childCont->isLogicVisible()) {
                childCont->applyLayout();
            }
        }
    }

    virtual void scale(int newDpi);
    ILayoutStrategy* getLayoutStrategy() const { return m_layoutStrategy.get(); }
	
	bool setProperty(const std::wstring& name, const vData& value) override;
    vData getProperty(const std::wstring& name) const override;


};

#endif // VCONTAINER_HPP