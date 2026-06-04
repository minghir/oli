#include <map>
#include <string>
#include <vector>
#include <memory>
#include "pugixml-1.15/src/pugixml.hpp"
#include "stringUtils.hpp"
#include "../../ConsoleManager.hpp"

class XmlCache {
private:
    // Stocăm documentele într-un map: calea_fisierului -> obiect_document
    std::map<std::string, std::shared_ptr<pugi::xml_document>> m_cache;

public:
    static XmlCache& getInstance() {
        static XmlCache instance;
        return instance;
    }
    /*
    // Întoarce documentul dacă există, altfel îl încarcă acum și îl salvează
    std::shared_ptr<pugi::xml_document> getXml(const std::string& path) {
        auto it = m_cache.find(path);
        if (it != m_cache.end()) {
            LOG_SUCCESS(L"Am incarcat din cache: " + str_to_wstr(path));
            return it->second;
        }
        else {
            LOG_WARNING(L"Am incarcat din fisier: " + str_to_wstr(path));
        }

        // Dacă nu e în cache, îl încărcăm
        auto doc = std::make_shared<pugi::xml_document>();
        pugi::xml_parse_result result = doc->load_file(path.c_str());

        if (!result) {
            LOG_ERROR(L"Eroare la parsarea XML-ului pentru cache: " + str_to_wstr(path));
            return nullptr;
        }

        m_cache[path] = doc;
        return doc;
    }
    */

    std::shared_ptr<pugi::xml_document> getXml(const std::string& path, bool forceReload = false) {
        if (forceReload) {
            m_cache.erase(path); // Forțăm eliminarea versiunii vechi
        }

        auto it = m_cache.find(path);
        if (it != m_cache.end()) {
            LOG_SUCCESS(L"Am incarcat din cache: " + str_to_wstr(path));
            return it->second;
        }
        else {
            LOG_WARNING(L"Am incarcat din fisier: " + str_to_wstr(path));
        }

        auto doc = std::make_shared<pugi::xml_document>();
        // Folosim pugi::parse_default pentru viteză sau poți adăuga pugi::parse_comments dacă ai nevoie
        pugi::xml_parse_result result = doc->load_file(path.c_str());

        if (!result) {
            LOG_ERROR(L"Eroare la încărcare XML: " + str_to_wstr(path));
            return nullptr;
        }

        m_cache[path] = doc;
        return doc;
    }

    // Opțional: Metodă să le pre-încarci pe toate la startul aplicației
    void preload(const std::vector<std::string>& paths) {
        for (const auto& path : paths) {
            getXml(path);
        }
    }
};