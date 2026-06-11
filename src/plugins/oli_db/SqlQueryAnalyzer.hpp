#ifndef SQLQUERYANALIZER_HPP
#define SQLQUERYANALIZER_HPP

#include <string>
#include <algorithm>
#include <iostream>

class SqlQueryAnalyzer {
public:
    SqlQueryAnalyzer(const std::wstring& query) : m_originalQuery(query) {
        // Pre-procesare: curățarea interogării pentru analiză
        m_cleanedQuery = cleanAndNormalizeQuery(query);
    }

    /**
     * @brief Returnează interogarea necesară pentru COUNT(*), eliminând ORDER BY, LIMIT și OFFSET.
     */
    std::wstring getBaseQueryForCount() const {
        std::wstring query = m_cleanedQuery;

        // Pasul 1: Elimină clauza ORDER BY (și tot ce urmează după ea)
        // Căutarea inversă de la finalul string-ului este o metodă comună pentru clauzele de final.
        size_t pos_order = query.rfind(L" ORDER BY ");
        if (pos_order != std::wstring::npos) {
            query.erase(pos_order);
        }

        // Pasul 2: Elimină clauza LIMIT/FETCH (și tot ce urmează după ea)
        // Deși este mai complex să știi exact unde se termină LIMIT/FETCH,
        // putem căuta simplu cuvintele cheie.

        // Căutăm LIMIT
        size_t pos_limit = query.rfind(L" LIMIT ");
        if (pos_limit != std::wstring::npos) {
            query.erase(pos_limit);
        }

        // Căutăm FETCH (pentru standardul SQL OFFSET/FETCH)
        size_t pos_fetch = query.rfind(L" FETCH ");
        if (pos_fetch != std::wstring::npos) {
            query.erase(pos_fetch);
        }

        // Rețineți: Acesta este un parser rudimentar și poate eșua în cazuri complexe (e.g., LIMIT în subquerry).

        // Elimină spațiile de la final, dacă rămân
        while (!query.empty() && iswspace(query.back())) {
            query.pop_back();
        }

        return query;
    }

    bool containsPaginationClause() const {
        // Folosește interogarea curățată (m_cleanedQuery) pentru a verifica
        return m_cleanedQuery.find(L" limit ") != std::wstring::npos ||
            m_cleanedQuery.find(L" fetch ") != std::wstring::npos;
    }


private:
    std::wstring m_originalQuery;
    std::wstring m_cleanedQuery;

    /**
     * @brief Convertește interogarea la lowercase și normalizează spațiile.
     */
    std::wstring cleanAndNormalizeQuery(const std::wstring& query) const {
        std::wstring cleaned = query;
        // 1. Convertire la lowercase
        //std::transform(cleaned.begin(), cleaned.end(), cleaned.begin(), ::towlower);

        // 2. Înlocuirea spațiilor multiple cu unul singur (Opțional, dar ajută)
        // Nu este trivial de făcut robust, dar pentru început, ne bazăm pe un singur spațiu
        // în jurul cuvintelor cheie (e.g., " limit ", " order by ").

        return cleaned;
    }
};

#endif
