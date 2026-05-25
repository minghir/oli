#pragma once
#include "vPanel.hpp"
#include "vRichEdit.hpp"
#include "Layouts/AnchorLayout.hpp"
#include "CodeLexer.hpp"
#include <memory>

class vCodeView : public vPanel {
private:
    vRichEdit* m_richEdit = nullptr;
    
    CodeLexer m_lexer;
    int m_fontSize = 12;
    const int m_gutterWidth = 50;
	std::wstring m_currentFilePath;
    std::wstring m_syntaxPath;

    void drawLineNumbers(HDC hdc);
public:
    vCodeView(HINSTANCE hInstance, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher)
        : vPanel(hInstance, id, x, y, width, height, dispatcher)
    {
        // Nu uita: vPanel va fi părintele pentru RichEdit
        //LOG_DEBUG(L"[vCodeView] Initializare vCodeView");
        //m_lexer.loadSyntaxes("olide\\syntaxes");
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
    void setText(const std::wstring& text) override {
        if (m_richEdit) {
            // 1. Dezactivăm temporar redesenarea (redresarea grafică) pentru a evita pâlpâitul (flicker)
            SendMessage(m_richEdit->getHandle(), WM_SETREDRAW, FALSE, 0);

            // 2. Setăm textul brut în controlul RichEdit
            m_richEdit->setText(text);

            // 3. Rulăm Lexer-ul pentru a colora textul conform limbajului curent detectat
            m_lexer.highlight(m_richEdit);

            // 4. Reactivăm redesenarea și forțăm un refresh vizual complet
            SendMessage(m_richEdit->getHandle(), WM_SETREDRAW, TRUE, 0);
            InvalidateRect(m_richEdit->getHandle(), NULL, TRUE);
        }
    }
    std::wstring getText() const { return m_richEdit ? m_richEdit->getText() : L""; }

    vRichEdit* getEditor() { return m_richEdit; }

    bool loadFromFile(const std::wstring& filePath);
    void setReadOnly(bool readOnly);

   
    void redrawGutter();

    void setSyntaxPath(const std::wstring& syntaxPath) {
        m_syntaxPath = syntaxPath;

        // 1. Încărcăm folderul de sintaxe (care populează mapa m_extMap)
        std::string pathAnsi(syntaxPath.begin(), syntaxPath.end());
        m_lexer.loadSyntaxes(pathAnsi);

        // 2. 🔥 REPARAT: Îi spunem lexerului să activeze limbajul bazat pe numele fișierului de sintaxă!
        // Dacă ai funcția setLanguageByFile, o putem păcăli trimițându-i un nume fictiv cu extensia corectă.
        // De exemplu, dacă calea este ".../oli.xml", putem folosi o extensie temporară sau o funcție directă.
        if (syntaxPath.find(L"oli.xml") != std::wstring::npos) {
            m_lexer.setLanguageByFile(L"dummy.oli"); // Forțează activarea sintaxei de Oli
        }
        else if (syntaxPath.find(L"cpp.xml") != std::wstring::npos) {
            m_lexer.setLanguageByFile(L"dummy.cpp"); // Forțează activarea sintaxei de C++
        }
        else {
            // Fallback: încearcă să detecteze limbajul direct prin calea fișierului
            m_lexer.setLanguageByFile(syntaxPath);
        }

        if (m_richEdit) {
            SendMessage(m_richEdit->getHandle(), WM_SETREDRAW, FALSE, 0);
            m_lexer.highlight(m_richEdit);
            SendMessage(m_richEdit->getHandle(), WM_SETREDRAW, TRUE, 0);
            InvalidateRect(m_richEdit->getHandle(), NULL, TRUE);
        }
    }

	void setFontSize(int size) override; // Suprascrie metoda de bază
	
	std::wstring getSyntaxPath() const { return m_syntaxPath; }
	
	bool setProperty(const std::wstring& name, const vData& value) override;
    vData getProperty(const std::wstring& name) const override;
};