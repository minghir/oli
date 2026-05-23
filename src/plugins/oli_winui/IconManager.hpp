#ifndef ICON_MANAGER_HPP
#define ICON_MANAGER_HPP

#include <windows.h>
#include <string>
#include <map>

// Structura cheii pentru a identifica unic o pictogramă
struct IconKey {
    std::wstring filePath;
    int width;
    int height;

    // Operator de comparație pentru a folosi IconKey ca cheie în std::map
    bool operator<(const IconKey& other) const {
        if (filePath != other.filePath) return filePath < other.filePath;
        if (width != other.width) return width < other.width;
        return height < other.height;
    }
};

class IconManager {
public:
    // Metoda statică pentru a obține instanța Singleton
    static IconManager& getInstance();

    // Nu permit copierea sau asignarea
    IconManager(const IconManager&) = delete;
    IconManager& operator=(const IconManager&) = delete;

    // Obține un handle HICON pentru o pictogramă dintr-un fișier.
    // Încarcă și cazează pictograma dacă nu există deja.
    HICON getIcon(const std::wstring& filePath, int width, int height);

    // Curăță toate pictogramele încărcate.
    void cleanup();

private:
    // Constructor privat (specific Singleton)
    IconManager() = default;

    // Destructor (va fi apelat la închiderea aplicației)
    ~IconManager();

    // Mapă pentru a stoca pictogramele încărcate
    std::map<IconKey, HICON> m_iconCache;
};

#endif // ICON_MANAGER_HPP