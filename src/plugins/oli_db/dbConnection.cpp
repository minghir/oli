#define _CRT_SECURE_NO_WARNINGS
#include "dbConnection.hpp"
#include "stringUtils.hpp"
#include <iostream>

/*
int vConTable::getColumnIndex(const std::wstring& name) const {
    std::wstring searchName = to_upper(wstr_trim(name));

    // 1. Verificăm dacă există un punct (format TABLE.COLUMN sau ALIAS.COLUMN)
    size_t dotPos = searchName.find(L'.');
    if (dotPos != std::wstring::npos) {
        std::wstring prefix = searchName.substr(0, dotPos);
        std::wstring actualColumn = searchName.substr(dotPos + 1);

        // Pregătim numele tabelului fără extensie pentru comparație (ex: "PERSOANE.DBF" -> "PERSOANE")
        std::wstring pureTableName = to_upper(tableName);
        size_t extPos = pureTableName.find(L".DBF");
        if (extPos != std::wstring::npos) pureTableName = pureTableName.substr(0, extPos);

        std::wstring upperAlias = to_upper(tableAlias);

        // Dacă prefixul corespunde alias-ului sau numelui tabelului
        if (prefix == upperAlias || prefix == pureTableName) {
            searchName = actualColumn; // Căutăm doar coloana efectivă
        }
        else {
            // Dacă prefixul nu ne aparține (e pentru alt tabel din JOIN), returnăm -1
            return -1;
        }
    }

    // 2. Căutarea clasică în vectorul de coloane
    for (int i = 0; i < (int)columns.size(); ++i) {
        if (columns[i] == searchName) return i;
    }

    return -1;
}
*/
/*/
int vConTable::getColumnIndex(const std::wstring& colName) const {
    std::wstring search = to_upper(colName);
    for (int i = 0; i < (int)columns.size(); ++i) {
        std::wstring current = to_upper(columns[i]);
        // Verificăm potrivire exactă (PERSOANE.ORAS == PERSOANE.ORAS)
        if (current == search) return i;

        // Verificăm potrivire parțială dacă search conține punct (PERSOANE.ORAS se găsește în ORAS?)
        size_t dotPos = search.find(L'.');
        if (dotPos != std::wstring::npos) {
            std::wstring suffix = search.substr(dotPos + 1);
            if (current == suffix) return i;
        }
    }
    return -1;
}
*/
int vConTable::getColumnIndex(const std::wstring& colName) const {
    std::wstring search = to_upper(wstr_trim(colName));

    // 1. CĂUTARE EXACTĂ (ex: "RUDE.NUME" == "RUDE.NUME")
    // Aceasta trebuie să fie prima, pentru că în JOIN coloanele sunt deja prefixate
    for (int i = 0; i < (int)columns.size(); ++i) {
        if (to_upper(columns[i]) == search) return i;
    }

    // 2. CĂUTARE INTELIGENTĂ (Dacă search e "NUME", dar coloana e "RUDE.NUME")
    // Verificăm dacă search este doar sufixul coloanei noastre
    for (int i = 0; i < (int)columns.size(); ++i) {
        std::wstring current = to_upper(columns[i]);
        size_t dotPos = current.find(L'.');
        if (dotPos != std::wstring::npos) {
            std::wstring suffix = current.substr(dotPos + 1);
            if (suffix == search) return i;
        }
    }

    return -1;
}