#ifndef VOLIKEYWORDS_HPP
#define VOLIKEYWORDS_HPP

#include <string>
#include <unordered_set>
#include <algorithm>

class vOliKeyWords {
private:
    // Cuvinte cheie SQL Standard
    static inline const std::unordered_set<std::wstring> RESERVED_SQL = {
        L"SELECT", L"FROM", L"WHERE", L"INSERT", L"UPDATE", L"DELETE",
        L"CREATE", L"DROP", L"TABLE", L"SCHEMA", L"AS", L"AND", L"OR",
        L"NOT", L"NULL", L"IN", L"INTO", L"VALUES", L"JOIN", L"ON"
    };

    // Comenzi interne de Shell (Slash Commands)
    static inline const std::unordered_set<std::wstring> SHELL_COMMANDS = {
        L"INFO", L"D",
        L"HELP", L"H", 
        L"SET", L"S",
        L"UNSET", L"U",
        L"ECHO", L"E", L"ECHO_DBG", L"ED", 
        L"IF",L"THEN",L"ELSE",L"ENDIF",
        L"WHILE",L"DO",L"ENDWHILE",
        L"FOR", L"TO", L"BY", L"ENDFOR",
        L"CYCLE",L"ENDCYCLE",
        L"RUN",L"R",
        L"SYS",
        L"PROC",
        L"FUNC",
        L"PLUGIN",
        L"QUIT", L"Q", L"EXIT",
        L"DUMP_MEM", L"DM", L"VARS",
        L"LIST",
        L"BREAK",
        L"CONTINUE",
        L"RETURN", L"RET",
        L"REPEAT", L"UNTIL", L"ENDREPEAT",
        L"SWITCH", L"CASE", L"DEFAULT", L"ENDSWITCH",
        L"CLEAR", L"CLS",
        L"TRACE",
        L"DEFINE", L"DEF",
        L"CONFIG", L"CONF"
    };

    static inline const std::unordered_set<std::wstring> DATA_TYPES = {
    L"INTEGER", L"DOUBLE", L"TEXT", L"DATE", L"BOOLEAN"
    };

    static inline const std::unordered_set<std::wstring> RESERVED = {
        L"SELECT", L"FROM", L"WHERE", L"AS", L"AND", L"OR", L"NOT",
        L"INSERT", L"INTO", L"VALUES", L"UPDATE", L"SET", L"DELETE",
        L"CREATE", L"TABLE", L"DROP", L"JOIN", L"ON", L"LIMIT"
    };

    static inline std::unordered_set<std::wstring> DYNAMIC_COMMANDS = {};

public:

    static void registerDynamicCommand(std::wstring name) {
        transformToUpper(name);
        // Ne asigurăm că începe cu '/' dacă așa ai decis sintaxa
        //if (name[0] != L'/') name = L"/" + name;
        DYNAMIC_COMMANDS.insert(name);
    }

    static bool isSqlKeyword(std::wstring word) {
        transformToUpper(word);
        return RESERVED_SQL.find(word) != RESERVED_SQL.end();
    }

    static bool isShellCommand(std::wstring word) {
        transformToUpper(word);
        // Verificăm atât în comenzile hardcodate, cât și în cele dinamice
        return (SHELL_COMMANDS.find(word) != SHELL_COMMANDS.end()) ||
            (DYNAMIC_COMMANDS.find(word) != DYNAMIC_COMMANDS.end());
    }

    static bool isInternalFixedCommand(std::wstring word) {
        transformToUpper(word);
        return SHELL_COMMANDS.find(word) != SHELL_COMMANDS.end();
    }

    static bool isReserved(const std::wstring& word) {
        return isSqlKeyword(word) || isShellCommand(word);
    }
    
    static bool isKeyword(const std::wstring& word) {
        std::wstring upper = word;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::towupper);
        return RESERVED.find(upper) != RESERVED.end();
    }

    static bool is(const std::wstring& word, const std::wstring& targetKeyword) {
        std::wstring w = word;
        std::transform(w.begin(), w.end(), w.begin(), ::towupper);
        return w == targetKeyword;
    }
    
    static bool isOliCommand(std::wstring word) {
        transformToUpper(word);
        return (SHELL_COMMANDS.count(word) > 0) || (DYNAMIC_COMMANDS.count(word) > 0);
    }

private:
    static void transformToUpper(std::wstring& s) {
        std::transform(s.begin(), s.end(), s.begin(), ::towupper);
    }
};

#endif