#ifndef FONT_MANAGER_HPP
#define FONT_MANAGER_HPP

#include <windows.h>
#include <string>
#include <map>
#include <memory> // Pentru std::unique_ptr (dacă decizi să stochezi handle-uri raționale)

enum class FontWeight : int
{
    DontCare = 0,   // FW_DONTCARE
    Thin = 100, // FW_THIN
    ExtraLight = 200, // FW_EXTRALIGHT / FW_ULTRALIGHT
    Light = 300, // FW_LIGHT
    Normal = 400, // FW_NORMAL / FW_REGULAR
    Medium = 500, // FW_MEDIUM
    SemiBold = 600, // FW_SEMIBOLD / FW_DEMIBOLD
    Bold = 700, // FW_BOLD
    ExtraBold = 800, // FW_EXTRABOLD / FW_ULTRABOLD
    Black = 900  // FW_HEAVY / FW_BLACK
};

inline int fontWeight(FontWeight w)
{
    return static_cast<int>(w);
}

// Folosim o structură pentru a identifica unic un font
struct FontKey {
    std::wstring faceName;
    int height;
    int weight; // FW_NORMAL, FW_BOLD, etc.
    bool italic;
    bool underline;
    bool strikeout;

    // Operator de comparație pentru a putea folosi FontKey ca cheie în std::map
    bool operator<(const FontKey& other) const {
        if (faceName != other.faceName) return faceName < other.faceName;
        if (height != other.height) return height < other.height;
        if (weight != other.weight) return weight < other.weight;
        if (italic != other.italic) return italic < other.italic;
        if (underline != other.underline) return underline < other.underline;
        return strikeout < other.strikeout;
    }
};

class FontManager {
public:
    // Metoda statică pentru a obține instanța Singleton
    static FontManager& getInstance();

    // Nu permit copierea sau asignarea (specific Singleton)
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;

    // Obține un handle HFONT pentru un font dat.
    // Dacă fontul există deja în cache, îl returnează pe cel existent.
    // Altfel, creează un nou font, îl cazează și îl returnează.
    HFONT getFont(const std::wstring& faceName, int height, int weight = FW_NORMAL, bool italic = false, bool underline = false, bool strikeout = false);
    //HFONT getScaledFont(const std::wstring& name, int baseSize, int dpi);
    HFONT getScaledFont(const std::wstring& name, int baseSize, int dpi, int weight = FW_NORMAL, bool italic = false, bool underline = false);
    // Curăță toate fonturile încărcate.
    void cleanup();

private:
    // Constructor privat (specific Singleton)
    FontManager() = default;

    // Destructor (va fi apelat la închiderea aplicației)
    ~FontManager();

    // Mapă pentru a stoca fonturile create, folosing FontKey pentru identificare
    // și HFONT (handle-ul GDI) pentru valoare.
    std::map<FontKey, HFONT> m_fontCache;
};

#endif // FONT_MANAGER_HPP