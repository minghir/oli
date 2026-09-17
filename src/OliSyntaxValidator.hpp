#pragma once
#include "OliSyntaxChecker.hpp"
#include "OliSyntaxRules.hpp"
#include <sstream>

class OliSyntaxValidator {
private:
    std::vector<std::unique_ptr<ISyntaxRule>> m_rules;

public:
    OliSyntaxValidator() {
        // Înregistrăm regulile în pipeline
        m_rules.push_back(std::make_unique<SetCommandRule>());
        m_rules.push_back(std::make_unique<StructureValidatorRule>());
        m_rules.push_back(std::make_unique<StrictVariableSyntaxRule>());
        m_rules.push_back(std::make_unique<LoopContextRule>());
        m_rules.push_back(std::make_unique<NestingBalanceRule>());
        m_rules.push_back(std::make_unique<InvalidLHSRule>());
        m_rules.push_back(std::make_unique<StringQuotesRule>());
        m_rules.push_back(std::make_unique<ConsecutiveOperatorSyntaxRule>());
    }

    bool validate(const std::wstring& source, std::vector<SyntaxError>& errors) {
        std::wstringstream ss(source);
        std::wstring line;
        int currentLineNum = 0;

        while (std::getline(ss, line)) {
            currentLineNum++;
            std::wstring cleanLine = trim(line);
            if (cleanLine.empty() || cleanLine[0] == L'#') continue;

            // Împărțim linia dacă conține mai multe comenzi separate prin ';'
            std::vector<std::wstring> subCommands = splitBySemicolon(cleanLine);

            for (const auto& subCmdStr : subCommands) {
                std::wstring trimmedCmd = trim(subCmdStr);
                if (trimmedCmd.empty()) continue;

                // Parsăm comanda individuală
                ShellCommand sc = vOliCommandParser::parse(trimmedCmd);

                // Rulăm toate regulile din pipeline pentru instrucțiunea curentă
                for (const auto& rule : m_rules) {
                    rule->check(sc, currentLineNum, line, errors);
                }
            }
        }

        // Faza finală: Verificăm echilibrul blocurilor la nivel global de fișier
        for (const auto& rule : m_rules) {
            rule->finalize(errors);
        }

        // Blocăm executarea doar dacă avem erori critice (OLI_ERROR)
        for (const auto& err : errors) {
            if (err.level == DiagnosticLevel::OLI_ERROR) {
                return false;
            }
        }

        return true; // Scriptul este executabil (poate avea doar avertismente)
    }

private:
    std::wstring trim(const std::wstring& str) {
        size_t first = str.find_first_not_of(L" \t\r\n");
        if (first == std::wstring::npos) return L"";
        size_t last = str.find_last_not_of(L" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    std::vector<std::wstring> splitBySemicolon(const std::wstring& line) {
        std::vector<std::wstring> result;
        std::wstring current;
        bool inQuotes = false;
        wchar_t quoteChar = L'\0';

        for (wchar_t ch : line) {
            if ((ch == L'"' || ch == L'\'') && !inQuotes) {
                inQuotes = true;
                quoteChar = ch;
            }
            else if (ch == quoteChar && inQuotes) {
                inQuotes = false;
            }

            if (ch == L';' && !inQuotes) {
                result.push_back(current);
                current.clear();
            }
            else {
                current += ch;
            }
        }
        if (!current.empty()) {
            result.push_back(current);
        }
        return result;
    }
};

/*
#pragma once

#include "OliSyntaxChecker.hpp"
#include "OliSyntaxRules.hpp"

class OliSyntaxValidator {
private:
    std::vector<std::unique_ptr<ISyntaxRule>> m_rules;

public:
    OliSyntaxValidator() {
        // Înregistrăm regulile în pipeline
        
        m_rules.push_back(std::make_unique<SetCommandRule>());
        m_rules.push_back(std::make_unique<StructureValidatorRule>());
        m_rules.push_back(std::make_unique<StrictVariableSyntaxRule>());
        m_rules.push_back(std::make_unique<LoopContextRule>());
        m_rules.push_back(std::make_unique<NestingBalanceRule>());
		m_rules.push_back(std::make_unique<InvalidLHSRule>());
        m_rules.push_back(std::make_unique<StringQuotesRule>());
        m_rules.push_back(std::make_unique<ConsecutiveOperatorSyntaxRule>());
        
        

    }

    bool validate(const std::wstring& source, std::vector<SyntaxError>& errors) {
        std::wstringstream ss(source);
        std::wstring line;
        int currentLineNum = 0;

        while (std::getline(ss, line)) {
            currentLineNum++;
            std::wstring cleanLine = line; // Se poate folosi trim-ul tău aici
            if (cleanLine.empty() || cleanLine[0] == L'#') continue;

            // Parsăm linia în structura ShellCommand utilizând parserul tău existent
            ShellCommand sc = vOliCommandParser::parse(cleanLine);

            // Rulăm toate regulile pentru linia curentă
            for (const auto& rule : m_rules) {
                rule->check(sc, currentLineNum, line, errors);
            }
        }
        for (const auto& rule : m_rules) {
            rule->finalize(errors);
        }

        return errors.empty(); // True dacă nu avem nicio eroare
    }
};
*/