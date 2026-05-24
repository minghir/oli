#include "CodeLexer.hpp"
#include "vRichEdit.hpp" // Header-ul unde ai clasa editorului
#include "stringUtils.hpp" // Presupunând că aici ai utf8_to_wstring
#include "XmlCache.hpp" // Presupunând că aici ai utf8_to_wstring
#include <filesystem>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

COLORREF CodeLexer::hexToColor(const std::string& hex) {
    if (hex.empty() || hex[0] != '#') return RGB(0, 0, 0);
    int r, g, b;
    if (sscanf_s(hex.c_str(), "#%02x%02x%02x", &r, &g, &b) == 3) return RGB(r, g, b);
    return RGB(0, 0, 0);
}

bool CodeLexer::loadLanguageFile(const std::string& path) {

    auto doc = XmlCache::getInstance().getXml(path);
    if (!doc) return false;

    //pugi::xml_document doc;
    //if (!doc.load_file(path.c_str())) return false;

    auto root = doc->child("SyntaxDefinition");
    if (root.empty()) return false;

    auto lang = std::make_shared<LangDefinition>();

    lang->name = utf8_to_wstring(root.attribute("name").as_string());
    lang->caseSensitive = root.child("Settings").attribute("caseSensitive").as_bool();

    std::wstringstream ssExt(utf8_to_wstring(root.attribute("extensions").as_string()));
    std::wstring ext;
    while (ssExt >> ext) lang->extensions.push_back(ext);

    for (auto styleNode : root.child("Styles").children("Style")) {
        std::string id = styleNode.attribute("id").as_string();
        LangStyle s;
        s.color = hexToColor(styleNode.attribute("color").as_string());
        s.bold = styleNode.attribute("bold").as_bool();
        s.startTag = utf8_to_wstring(styleNode.attribute("start").as_string());
        s.endTag = utf8_to_wstring(styleNode.attribute("end").as_string());

        if (id == "keyword") {
            std::wstringstream ss(utf8_to_wstring(styleNode.text().get()));
            std::wstring word;
            while (ss >> word) {
                if (!lang->caseSensitive) std::transform(word.begin(), word.end(), word.begin(), ::towlower);
                s.words.insert(word);
            }
            lang->keywordStyle = s;
        }
        else if (id == "number") {
            s.isNumber = true;
            lang->numberStyle = s;
        }
        else {
            lang->blockStyles.push_back(s);
        }
    }

    for (const auto& e : lang->extensions) m_extMap[e] = lang;
    return true;
}

void CodeLexer::highlight(vRichEdit* editor) {
    if (!m_currentLang || !editor) return;

    std::wstring text = editor->getText();
    if (text.empty()) return;

    text.erase(std::remove(text.begin(), text.end(), L'\r'), text.end());

    editor->freeze();
    editor->setTextColorRange(0, (int)text.length(), RGB(0, 0, 0), false);

    for (size_t i = 0; i < text.length(); ) {
        bool matchedBlock = false;

        // Fix C3536: Folosim indexare clasică pentru blockStyles
        for (size_t s = 0; s < m_currentLang->blockStyles.size(); ++s) {
            const auto& style = m_currentLang->blockStyles[s];
            if (style.startTag.empty()) continue;

            if (i + style.startTag.length() <= text.length() &&
                text.compare(i, style.startTag.length(), style.startTag) == 0) {

                size_t start = i;
                size_t end = std::wstring::npos;

                if (style.endTag.empty()) {
                    end = text.find(L'\n', i);
                }
                else {
                    end = text.find(style.endTag, i + style.startTag.length());
                    if (end != std::wstring::npos) end += style.endTag.length();
                }

                if (end == std::wstring::npos) end = text.length();

                editor->setTextColorRange((int)start, (int)end, style.color, style.bold);
                i = end;
                matchedBlock = true;
                break;
            }
        }

        if (matchedBlock) continue;

        // Logica de cuvinte (Keywords/Numbers)
        if (iswalnum(text[i]) || text[i] == L'_' || text[i] == L'$') {
            size_t start = i;
            while (i < text.length() && (iswalnum(text[i]) || text[i] == L'_' || text[i] == L'$')) i++;

            std::wstring word = text.substr(start, i - start);
            if (iswdigit(word[0])) {
                editor->setTextColorRange((int)start, (int)i, m_currentLang->numberStyle.color, m_currentLang->numberStyle.bold);
            }
            else {
                if (!m_currentLang->caseSensitive) std::transform(word.begin(), word.end(), word.begin(), ::towlower);
                if (m_currentLang->keywordStyle.words.count(word)) {
                    editor->setTextColorRange((int)start, (int)i, m_currentLang->keywordStyle.color, m_currentLang->keywordStyle.bold);
                }
            }
            continue;
        }
        i++;
    }
    editor->unfreeze();
}

void CodeLexer::setLanguageByFile(const std::wstring& filePath) {
    // 1. Extragem extensia folosind std::filesystem
    // path(filePath).extension() returnează ceva de genul ".oli" sau ".cpp"
    std::wstring ext = std::filesystem::path(filePath).extension().wstring();

    // 2. Căutăm extensia în harta noastră de limbaje (m_extMap)
    auto it = m_extMap.find(ext);

    if (it != m_extMap.end()) {
        // Am găsit limbajul! (it->second este un shared_ptr la LangDefinition)
        m_currentLang = it->second;
        // LOG_SUCCESS(L"[CodeLexer] Limbaj detectat: " + m_currentLang->name);
    }
    else {
        // Nu avem definiție pentru această extensie
        m_currentLang = nullptr;
        // LOG_WARNING(L"[CodeLexer] Limbaj necunoscut pentru extensia: " + ext);
    }
}

void CodeLexer::loadSyntaxes(const std::string& folderPath) {
    namespace fs = std::filesystem;

    try {
        // Convertim string-ul brut într-un obiect path pentru a-i folosi metodele deștepte
        fs::path p(folderPath);
        fs::path finalFolder = p;

        // 🔥 LOGICA DE EXTRAGERE: Dacă este un fișier, luăm folderul părinte!
        if (fs::is_regular_file(p)) {
            finalFolder = p.parent_path();
            LOG_WARNING(L"[CodeLexer] Calea primită indică un fișier. Am extras folderul părinte: " + finalFolder.wstring());
        }

        // 1. Verificăm dacă folderul rezultat există cu adevărat
        if (!fs::exists(finalFolder) || !fs::is_directory(finalFolder)) {
            LOG_ERROR(L"[CodeLexer] Folderul de sintaxe nu a fost găsit sau este invalid: " + finalFolder.wstring());
            return;
        }
        else {
            LOG_INFO(L"[CodeLexer] Încep scanarea folderului de sintaxe: " + finalFolder.wstring());
        }

        // 2. Scanăm fiecare fișier din folderul extras
        auto it = fs::directory_iterator(finalFolder);
        for (const auto& entry : it) {
            // 3. Ne interesează doar fișierele .xml
            if (entry.is_regular_file() && entry.path().extension() == ".xml") {

                std::string filePath = entry.path().string();

                // Încărcăm definiția de limbaj folosind metoda loadLanguageFile
                if (loadLanguageFile(filePath)) {
                    LOG_SUCCESS(L"[CodeLexer] Sintaxă încărcată cu succes: " + entry.path().filename().wstring());
                }
                else {
                    LOG_ERROR(L"[CodeLexer] Eroare la încărcarea sintaxei: " + entry.path().filename().wstring());
                }
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        // LOG_ERROR(L"[CodeLexer] Eroare sistem la scanarea folderului... ");
    }
}