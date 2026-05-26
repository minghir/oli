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
    int m_gutterWidth = 0;
	std::wstring m_currentFilePath;
    std::wstring m_syntaxPath;

    void drawLineNumbers(HDC hdc);
	int calculateGutterWidth();
public:
    vCodeView(HINSTANCE hInstance, const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher)
        : vPanel(hInstance, id, x, y, width, height, dispatcher)
    {
        // Nu uita: vPanel va fi părintele pentru RichEdit
        //LOG_DEBUG(L"[vCodeView] Initializare vCodeView");
        //m_lexer.loadSyntaxes("olide\\syntaxes");
    }
    

    void create(HWND parent) override;


    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
    // Proxy methods: redirecționăm apelurile către m_richEdit-ul intern
    void setText(const std::wstring& text) override ;
    std::wstring getText() const { return m_richEdit ? m_richEdit->getText() : L""; }

    vRichEdit* getEditor() { return m_richEdit; }

    bool loadFromFile(const std::wstring& filePath);
    void setReadOnly(bool readOnly);

   
    void redrawGutter();

    void setSyntaxPath(const std::wstring& syntaxPath);

	void setFontSize(int size) override; // Suprascrie metoda de bază
	void scaleFont(int newDpi) override;
	
	std::wstring getSyntaxPath() const { return m_syntaxPath; }
	
	bool setProperty(const std::wstring& name, const vData& value) override;
    vData getProperty(const std::wstring& name) const override;
	
	
	 void scale(int newDpi) override {
        // Apelăm implementarea din vContainer, care conține bucla de propagare la copii
        vPanel::scale(newDpi);
        
    }
	
		void moveAndResize(int x, int y, int width, int height) ;
};