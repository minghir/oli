#ifndef CODELEXER_HPP
#define CODELEXER_HPP

#pragma once
#include <Windows.h> // Esențial pentru COLORREF
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <memory>
//#include "pugixml.hpp"
#include "pugixml-1.15/src/pugixml.hpp"

// Forward declaration pentru vRichEdit dacă nu vrei să incluzi tot header-ul aici
class vRichEdit;

struct LangStyle {
    COLORREF color;      // Membrul pe care compilatorul nu îl vedea
    bool bold;
    std::wstring startTag;
    std::wstring endTag;
    std::set<std::wstring> words;
    bool isNumber = false;

    // Constructor pentru a asigura inițializarea corectă
    LangStyle() : color(RGB(0, 0, 0)), bold(false), isNumber(false) {}
};

struct LangDefinition {
    std::wstring name;
    std::vector<std::wstring> extensions;
    bool caseSensitive;
    std::vector<LangStyle> blockStyles;
    LangStyle keywordStyle;
    LangStyle numberStyle;
};

class CodeLexer {
private:
    std::unordered_map<std::wstring, std::shared_ptr<LangDefinition>> m_extMap;
    std::shared_ptr<LangDefinition> m_currentLang = nullptr;

    COLORREF hexToColor(const std::string& hex);

public:
    CodeLexer() = default;
    void loadSyntaxes(const std::string& folderPath);
    bool loadLanguageFile(const std::string& path);
    void setLanguageByFile(const std::wstring& filePath);
    void highlight(vRichEdit* editor);
};

#endif