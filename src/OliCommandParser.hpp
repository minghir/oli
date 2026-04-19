
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

            if (c == L'"') {
                inQuotes = !inQuotes;
                tok += c;
                if (!inQuotes) flush();
                continue;
            }

            if (!inQuotes) {
                // 1. Operatori compuși
                if (i + 1 < line.size()) {
                    std::wstring two = line.substr(i, 2);
                    if (two == L"==" || two == L"!=" || two == L">=" || two == L"<=" ||
                        two == L"&&" || two == L"||" || two == L"++" || two == L"--" ||
                        two == L"**" || two == L"??" ||
                        two == L"+=" || two == L"-=" || two == L"*=" || two == L"/=") {
                        flush();
                        tokens.push_back(two);
                        i++; continue;
                    }
                }

               
                // 2. LOGICĂ PUNCT (FLOAT vs DOT)
                if (c == L'.') {
                    bool isFloat = false;

                    // Un punct este zecimal DOAR DACĂ este înconjurat de cifre
                    // Exemplu: "2.5" -> DA | "c1.locatie" -> NU (chiar dacă are '1' în spate)
                    if (i > 0 && iswdigit(line[i - 1]) && i + 1 < line.size() && iswdigit(line[i + 1])) {
                        isFloat = true;
                    }

                    // EXCEPȚIE: Dacă deja am început să scriem un număr (ex: "123.")
                    // Verificăm dacă TOATĂ porțiunea 'tok' de până acum este formată doar din cifre
                    if (!tok.empty() && std::all_of(tok.begin(), tok.end(), ::iswdigit)) {
                        if (i + 1 < line.size() && iswdigit(line[i + 1])) {
                            isFloat = true;
                        }
                    }

                    if (isFloat) {
                        tok += c;
                    }
                    else {
                        flush();
                        tokens.push_back(L"."); // SĂ FIE DOT OPERATOR!
                    }
                    continue;
                }

                // 3. Separatori (AM SCOS PUNCTUL DE AICI!)
                if (wcschr(L"=+-*<>|;()[]{},:%/!^", c)) {
                    flush();
                    tokens.push_back(std::wstring(1, c));
                    continue;
                }

                if (iswspace(c)) {
                    flush();
                    continue;
                }
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
