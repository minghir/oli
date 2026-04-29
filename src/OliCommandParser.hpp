
#ifndef VOLICOMMANDPARSER_HPP
#define VOLICOMMANDPARSER_HPP

#include <string>
#include <vector>

#include "OliKeyWords.hpp"
#include "ConsoleManager.hpp"

struct ShellCommand {
    std::wstring name;
    std::vector<std::wstring> args;
    bool isValid = false;
};

inline void printShellCommandStr(const ShellCommand& cmd) {
    if (!cmd.isValid) {
        std::wcout << L"[ShellCommand] Status: INVALID" << std::endl;
        return;
    }

    std::wstringstream ss;
    ss << L"[ShellCommand] " << cmd.name << L" (" << cmd.args.size() << L" args):" << std::endl;

    for (size_t i = 0; i < cmd.args.size(); ++i) {
        // Punem argumentele între ghilimele ca să vedem spațiile goale
        ss << L"  [" << i << L"]: L\"" << cmd.args[i] << L"\"" << std::endl;
    }

    std::wcout << ss.str() << std::endl;
}

class vOliCommandParser {
private:
    
    
public:
   
/*
    static std::vector<std::wstring> tokenize(const std::wstring& line) {
        std::vector<std::wstring> tokens;
        std::wstring tok;
        bool inQuotes = false;

        auto flush = [&]() {
            if (!tok.empty()) {
                tokens.push_back(tok);
                tok.clear();
            }
            };

        for (size_t i = 0; i < line.size(); ++i) {
            wchar_t c = line[i];

            // --- 1. GESTIONARE GHILIMELE (CU SUPORT PENTRU ESCAPE \") ---
            if (c == L'"') {
                // Verificăm dacă ghilimeaua este „escapată” (precedată de \)
                bool isEscaped = (i > 0 && line[i - 1] == L'\\');

                if (isEscaped) {
                    // Dacă e escapată, face parte din conținutul token-ului
                    tok += c;
                }
                else {
                    // Dacă nu e escapată, este un delimitator de string (început sau sfârșit)
                    inQuotes = !inQuotes;
                    tok += c;
                    // Dacă tocmai am închis o pereche de ghilimele, salvăm string-ul ca un token întreg
                    if (!inQuotes) flush();
                }
                continue;
            }

            // --- 2. LOGICĂ PENTRU TOKENI ÎN AFARA GHILIMELELOR ---
            if (!inQuotes) {
                // A. Operatori compuși (verificăm 2 caractere înainte)
                if (i + 1 < line.size()) {
                    std::wstring two = line.substr(i, 2);
                    if (two == L"==" || two == L"!=" || two == L">=" || two == L"<=" ||
                        two == L"&&" || two == L"||" || two == L"++" || two == L"--" ||
                        two == L"**" || two == L"??" ||
                        two == L"+=" || two == L"-=" || two == L"*=" || two == L"/=") {
                        flush();
                        tokens.push_back(two);
                        i++; // Sărim peste al doilea caracter al operatorului
                        continue;
                    }
                }

                // B. Logica de punct (Diferențiere între FLOAT și DOT operator)
                if (c == L'.') {
                    bool isFloat = false;
                    // Cazul: 3.14 (cifre de ambele părți)
                    if (i > 0 && iswdigit(line[i - 1]) && i + 1 < line.size() && iswdigit(line[i + 1])) {
                        isFloat = true;
                    }
                    // Cazul: .5 sau număr început anterior care primește zecimală
                    else if (!tok.empty() && std::all_of(tok.begin(), tok.end(), ::iswdigit)) {
                        if (i + 1 < line.size() && iswdigit(line[i + 1])) {
                            isFloat = true;
                        }
                    }

                    if (isFloat) {
                        tok += c;
                    }
                    else {
                        flush();
                        tokens.push_back(L"."); // Îl tratăm ca operator de acces (DOT)
                    }
                    continue;
                }

                // C. Separatori și Operatori simpli
                if (wcschr(L"=+-*<>|;()[]{},:%/!^", c)) {
                    flush();
                    tokens.push_back(std::wstring(1, c));
                    continue;
                }

                // D. Spații albe (delimitatori de tokeni)
                if (iswspace(c)) {
                    flush();
                    continue;
                }
            }

            // --- 3. CONSTRUIRE TOKEN ---
            // Dacă am ajuns aici, caracterul face parte din token-ul curent (literal, variabilă, etc.)
            tok += c;
        }

        flush(); // Salvăm ultimul token rămas în buffer
        return tokens;
    }
*/

static std::vector<std::wstring> tokenize(const std::wstring& line) {
    std::vector<std::wstring> tokens;
    std::wstring tok;
    bool inQuotes = false;
    wchar_t quoteChar = L'\0';

    auto flush = [&]() {
        if (!tok.empty()) {
            tokens.push_back(tok);
            tok.clear();
        }
    };

    for (size_t i = 0; i < line.size(); ++i) {
        wchar_t c = line[i];

        // 1. GESTIONARE STRING-URI (Prioritate maximă)
        if ((c == L'"' || c == L'\'') && (i == 0 || line[i - 1] != L'\\')) {
            if (!inQuotes) {
                inQuotes = true;
                quoteChar = c;
            } else if (c == quoteChar) {
                inQuotes = false;
                quoteChar = L'\0';
                // NU dăm flush aici dacă vrem să permitem concatenări de tipul "A"BC
            }
            tok += c;
            continue;
        }

        // 2. DACĂ SUNTEM ÎN GHILIMELE, ACCEPTĂM ORICE CARACTER
        if (inQuotes) {
            tok += c;
            continue;
        }

        // 3. LOGICĂ PENTRU EXTERIORUL GHILIMELELOR
        if (iswspace(c)) {
            flush();
            continue;
        }

        // Operatori compuși (==, !=, etc.)
        if (i + 1 < line.size()) {
            std::wstring two = line.substr(i, 2);
            if (two == L"==" || two == L"!=" || two == L">=" || two == L"<=" ||
                two == L"&&" || two == L"||") {
                flush();
                tokens.push_back(two);
                i++; continue;
            }
        }

        // Separatori (virgula, paranteze, etc.)
        if (wcschr(L"=+-*<>|;()[]{},:%/!^", c)) {
            flush();
            tokens.push_back(std::wstring(1, c));
            continue;
        }

        tok += c;
    }
    flush();
    return tokens;
}

    static ShellCommand parse(const std::wstring& line) {
        ShellCommand cmd;
        if (line.empty()) return cmd;

        auto tokens = tokenize(line);
        if (tokens.empty()) return cmd;

        // Numele comenzii este întotdeauna primul token acum (ex: SET)
        cmd.name = tokens[0];

        // Transformăm în Upper pentru a verifica în lista de keywords
        std::wstring upperName = cmd.name;
        std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::towupper);

        if (!vOliKeyWords::isShellCommand(upperName)) {
            // Dacă nu e în listă, nu e o comandă validă (poate e o expresie)
            return cmd;
        }

        // Argumentele sunt restul token-urilor
        for (size_t i = 1; i < tokens.size(); ++i) {
            cmd.args.push_back(tokens[i]);
        }

        cmd.isValid = true;
        return cmd;
    }
};

#endif
