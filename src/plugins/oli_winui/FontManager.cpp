#include "FontManager.hpp"
#include "../../ConsoleManager.hpp" // Pentru logare

// Inițializarea statică a instanței (va fi creată la primul apel getInstance)
FontManager& FontManager::getInstance() {
    static FontManager instance; // Instanța este creată o singură dată (Magic Statics)
    return instance;
}

// Destructorul - responsabil cu eliberarea resurselor
FontManager::~FontManager() {
    cleanup(); // Asigură că toate fonturile sunt eliberate la distrugerea instanței
}

HFONT FontManager::getFont(const std::wstring& faceName, int height, int weight, bool italic, bool underline, bool strikeout) {
    FontKey key = { faceName, height, weight, italic, underline, strikeout };

    // Caută fontul în cache
    auto it = m_fontCache.find(key);
    if (it != m_fontCache.end()) {
        //ConsoleManager::getInstance().log(L"[FontManager] Returnează font existent pentru '" + faceName + L"' H:" + std::to_wstring(height));
        return it->second; // Fontul a fost găsit, returnează-l
    }

    // Fontul nu există în cache, creează-l
    HFONT hFont = CreateFontW(
        height,                // Înălțimea fontului
        0,                     // Lățimea medie a caracterelor (0 = Windows alege)
        0,                     // Unghiul de escapement
        0,                     // Unghiul de orientare
        weight,                // Grosimea fontului (FW_NORMAL, FW_BOLD, etc.)
        italic,                // Italic
        underline,             // Subliniat
        strikeout,             // Tăiat (strikeout)
        DEFAULT_CHARSET,       // Setul de caractere
        OUT_DEFAULT_PRECIS,    // Precizia de ieșire
        CLIP_DEFAULT_PRECIS,   // Precizia de clipping
        CLEARTYPE_QUALITY,     // Calitatea ieșirii (sau DEFAULT_QUALITY)
        FF_DONTCARE | FIXED_PITCH, // Familia de fonturi și pitch
        faceName.c_str()       // Numele fontului
    );

    if (hFont) {
        m_fontCache[key] = hFont; // Adaugă fontul în cache
    //    ConsoleManager::getInstance().log(L"[FontManager] Creat și adăugat font nou pentru '" + faceName + L"' H:" + std::to_wstring(height));
    }
    else {
        LOG_ERROR(L"[ERROR] FontManager: Nu s-a putut crea fontul '" + faceName + L"' H:" + std::to_wstring(height) + L". Eroare: " + std::to_wstring(GetLastError()));
    }

    return hFont;
}
/*
HFONT FontManager::getScaledFont(const std::wstring& name, int baseSize, int dpi) {
    // 1. Calculăm înălțimea scalată (folosind MulDiv pentru precizie)
    // Folosim -MulDiv pentru a potrivi mărimea caracterului, nu a celulei
    int scaledHeight = -MulDiv(baseSize, dpi, 72);

    // 2. Apelăm getFont. Aceasta va folosi structura FontKey,
    // va verifica m_fontCache și va returna fontul (existent sau nou).
    return getFont(name, scaledHeight, FW_NORMAL, false, false, false);
}
*/
/*
HFONT FontManager::getScaledFont(const std::wstring& name, int baseSize, int dpi,
    int weight, bool italic, bool underline) {
    int scaledHeight = -MulDiv(baseSize, dpi, 72);
    // Acum transmitem parametrii primiți, nu unii ficși
    return getFont(name, scaledHeight, weight, italic, underline, false);
}
*/

HFONT FontManager::getScaledFont(const std::wstring& name, int baseSize, int dpi,
    int weight, bool italic, bool underline) {

    int scaledHeight = -MulDiv(baseSize, dpi, 72);

    // LOG CRITIC
    //LOG_DEBUG(L"[FontManager] Request: " + name + L" | BaseSize: " + std::to_wstring(baseSize) +
    //    L" | DPI: " + std::to_wstring(dpi) + L" | Result Height: " + std::to_wstring(scaledHeight));

    return getFont(name, scaledHeight, weight, italic, underline, false);
}

/*
HFONT FontManager::getScaledFont(const std::wstring& name, int baseSize, int dpi) {
    // 1. Creăm o cheie unică pentru cache (Ex: "Arial_12_144")
    std::wstring key = name + L"_" + std::to_wstring(baseSize) + L"_" + std::to_wstring(dpi);

    // 2. Verificăm dacă am creat deja acest font
    auto it = m_fontCache.find(key);
    if (it != m_fontCache.end()) {
        return it->second;
    }

    // 3. Dacă nu există, îl creăm
    int scaledHeight = -MulDiv(baseSize, dpi, 72); // 72 pentru puncte (pt)

    LOGFONT lf = {};
    lf.lfHeight = scaledHeight;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET; // Recomandat pentru suport caractere speciale
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, name.c_str());

    HFONT hFont = CreateFontIndirect(&lf);

    // 4. Îl salvăm în cache pentru utilizări viitoare
    if (hFont) {
        m_fontCache[key] = hFont;
    }

    return hFont;
}
*/
void FontManager::cleanup() {
   // ConsoleManager::getInstance().log(L"[FontManager] Curățare resurse fonturi...");
    for (auto const& [key, hFont] : m_fontCache) {
        if (hFont) {
            DeleteObject(hFont); // Eliberează resursa GDI
        }
    }
    m_fontCache.clear(); // Golește cache-ul
 //   ConsoleManager::getInstance().log(L"[FontManager] Toate fonturile au fost eliberate.");
}