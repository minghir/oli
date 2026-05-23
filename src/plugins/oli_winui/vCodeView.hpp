#pragma once
#include "vPanel.hpp"
#include "vRichEdit.hpp"
#include "Layouts/AnchorLayout.hpp"
#include <memory>

class vCodeView : public vPanel {
private:
    vRichEdit* m_richEdit = nullptr;
    // vLineGutter* m_lineGutter = nullptr; // Viitorul control pentru numere

    int m_fontSize = 12;
    const int m_gutterWidth = 50;


    void drawLineNumbers(HDC hdc);
public:
    vCodeView(HINSTANCE hInstance, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher)
        : vPanel(hInstance, id, x, y, width, height, dispatcher)
    {
        // Nu uita: vPanel va fi părintele pentru RichEdit
    }
    /*
    void create(HWND parent) {
        vPanel::create(parent);
        //setLayoutStrategy(std::make_unique<AnchorLayout>());
     
        // 1. Creăm RichEdit-ul ca fiu al acestui Panel
        auto rich = std::make_unique<vRichEdit>(m_hInstance, m_id + "_edit", 50, 0, m_width, m_height, getEventDispatcher());
        m_richEdit = rich.get();
        //m_richEdit->setHeightMode(SizeMode::FILL);
        //m_richEdit->setWidthMode(SizeMode::FILL);
        m_richEdit->setFontSize(m_fontSize);
        // Dacă adăugăm Gutter-ul mai târziu, aici vom ajusta X-ul și lățimea
        // rich->setX(40); 
        // rich->setWidth(m_width - 40);

        this->addChild(m_id + "_edit", std::move(rich));

        // IMPORTANT: Activează mesajele de scroll în RichEdit
        // Altfel, Panel-ul nu va ști când să redeseneze numerele liniilor
        if (m_richEdit->getHandle()) {
            SendMessage(m_richEdit->getHandle(), EM_SETEVENTMASK, 0, ENM_SCROLL | ENM_CHANGE);
        }
        applyLayout();
    }
    */

    void create(HWND parent) override;


    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
    // Proxy methods: redirecționăm apelurile către m_richEdit-ul intern
    void setText(const std::wstring& text) { if (m_richEdit) m_richEdit->setText(text); }
    std::wstring getText() const { return m_richEdit ? m_richEdit->getText() : L""; }

    vRichEdit* getEditor() { return m_richEdit; }

    bool loadFromFile(const std::wstring& filePath);
    void setReadOnly(bool readOnly);

    void setFontSize(int size);
    void redrawGutter();
};