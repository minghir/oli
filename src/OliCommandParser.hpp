
#ifndef VOLICOMMANDPARSER_HPP
#define VOLICOMMANDPARSER_HPP

#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <sstream>
#include <iostream>
#include "OliKeyWords.hpp"
#include "ConsoleManager.hpp"

struct ShellCommand {
    std::wstring name;
    std::vector<std::wstring> args;
    bool isValid = false;
};

inline void printShellCommandStr(const ShellCommand& cmd) {
    if (!cmd.isValid) {
        LOG_DEBUG(L"[ShellCommand] Status: INVALID");
        return;
    }

    std::wstringstream ss;
    ss << L"[ShellCommand] " << cmd.name << L" (" << cmd.args.size() << L" args):" << std::endl;

    for (size_t i = 0; i < cmd.args.size(); ++i) {
        ss << L"  [" << i << L"]: L\"" << cmd.args[i] << L"\"" << std::endl;
    }

    LOG_DEBUG(ss.str());
}

class vOliCommandParser {
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

            // --- 1. GESTIONARE GHILIMELE ---
            if (c == L'"') {
                bool isEscaped = (i > 0 && line[i - 1] == L'\\');
                if (isEscaped) {
                    tok += c;
                }
                else {
                    inQuotes = !inQuotes;
                    tok += c;
                    if (!inQuotes) flush();
                }
                continue;
            }

            // --- 2. LOGICĂ PENTRU TOKENI ÎN AFARA GHILIMELELOR ---
            if (!inQuotes) {
                // A. Operatori compuși (2 caractere)
                if (i + 1 < line.size()) {
                    std::wstring two = line.substr(i, 2);
                    if (two == L"==" || two == L"!=" || two == L">=" || two == L"<=" ||
                        two == L"&&" || two == L"||" || two == L"++" || two == L"--" ||
                        two == L"**" || two == L"??" || two == L".." ||
                        two == L"<<" || two == L">>" || // <-- ADĂUGAT: Bitwise Shift
                        two == L"+=" || two == L"-=" || two == L"*=" || two == L"/=") {
                        flush();
                        tokens.push_back(two);
                        i++;
                        continue;
                    }
                }

                // B. Logică pentru FLOAT vs DOT
                if (c == L'.') {
                    bool isFloat = false;
                    if (i > 0 && iswdigit(line[i - 1]) && i + 1 < line.size() && iswdigit(line[i + 1])) {
                        isFloat = true;
                    }
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
                        tokens.push_back(L".");
                    }
                    continue;
                }

                // C. Separatori și Operatori simpli
                // Am adăugat '&' (AND) și '~' (NOT/Invert) în listă. 
                // '|' și '^' erau deja prezente.
                if (wcschr(L"=+-*<>|;()[]{},:%/!^&~", c)) {
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
    /*
    static ShellCommand parse(const std::wstring& line) {
        ShellCommand cmd;
        if (line.empty()) return cmd;

        auto tokens = tokenize(line);
        if (tokens.empty()) return cmd;

        cmd.name = tokens[0];
        std::wstring upperName = cmd.name;
        std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::towupper);

        if (!vOliKeyWords::isShellCommand(upperName)) {
            return cmd;
        }

        for (size_t i = 1; i < tokens.size(); ++i) {
            cmd.args.push_back(tokens[i]);
        }

        cmd.isValid = true;
        return cmd;
    }
    */

    static ShellCommand parse(const std::wstring& line) {
        ShellCommand cmd;
        if (line.empty()) return cmd;

        auto tokens = tokenize(line);
        if (tokens.empty()) return cmd;

        cmd.name = tokens[0];

        // COLECTĂM TOATE ARGUMENTELE, indiferent de tipul comenzii
        for (size_t i = 1; i < tokens.size(); ++i) {
            cmd.args.push_back(tokens[i]);
        }

        std::wstring upperName = cmd.name;
        std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::towupper);

        // Verificăm validitatea doar pentru a seta flag-ul isValid (pentru ramurile IF/SET/ECHO)
        if (vOliKeyWords::isShellCommand(upperName)) {
            cmd.isValid = true;
        }

        return cmd;
    }
};

#endif

