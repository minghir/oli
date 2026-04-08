
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
                continue;
            }

            if (!inQuotes) {
                // 1. Operatori compuși
                if (i + 1 < line.size()) {
                    std::wstring two = line.substr(i, 2);
                    if (two == L"==" || two == L"!=" || two == L">=" || two == L"<=" ||
                        two == L"&&" || two == L"||") {
                        flush();
                        tokens.push_back(two);
                        i++; continue;
                    }
                }

                // 2. Separatori (fără '/')
                if (wcschr(L"=+-*<>|;()[]{},:%", c)) {
                    flush();
                    tokens.push_back(std::wstring(1, c));
                    continue;
                }

                if (iswspace(c)) {
                    flush();
                    continue;
                }

                // 3. Logica REPARATĂ pentru '/'
                if (c == L'/') {
                    // Verificăm dacă '/' este operator:
                    // Este operator dacă în spate avem ceva care NU este spațiu 
                    // ȘI nu suntem la începutul liniei.
                    if (!tok.empty()) {
                        // Avem ceva lipit de el în stânga (ex: PI()/... sau 10/...)
                        flush();
                        tokens.push_back(L"/");
                        continue;
                    }
                    else {
                        // tok este gol. 
                        // Verificăm dacă suntem chiar la început sau după un spațiu (comanda)
                        // SAU dacă ultimul token a fost o paranteză închisă (caz special: PI() /PI)
                        if (!tokens.empty() && tokens.back() == L")") {
                            tokens.push_back(L"/");
                            continue;
                        }

                        // Altfel, este început de comandă (/if, /echo)
                        tok += c;
                        continue;
                    }
                }
            }
            tok += c;
        }
        flush();
        return tokens;
    }
    
    static ShellCommand parse(const std::wstring& line) {
        ShellCommand cmd;
        if (line.empty() || line[0] != L'/') return cmd;

        auto tokens = tokenize(line);
        if (tokens.empty()) return cmd;

        // --- REPARARE AICI ---
        size_t startIndex = 0;
        if (tokens[0] == L"/" && tokens.size() > 1) {
            cmd.name = L"/" + tokens[1]; // Lipim / de echo
            startIndex = 2;              // Argumentele încep de la al treilea token
        }
        else {
            cmd.name = tokens[0];
            startIndex = 1;
        }
        // ---------------------

        if (!vOliKeyWords::isShellCommand(cmd.name)) {
            LOG_ERROR(L"Unknown command: " + cmd.name);
            return cmd;
        }

        for (size_t i = startIndex; i < tokens.size(); ++i)
            cmd.args.push_back(tokens[i]);

        cmd.isValid = true;
        return cmd;
    }
};

#endif
