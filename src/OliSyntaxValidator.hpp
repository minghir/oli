#pragma once
#include "OliSyntaxChecker.hpp"
#include "OliSyntaxRules.hpp"
#include <sstream>

class OliSyntaxValidator {
private:
    std::vector<std::unique_ptr<ISyntaxRule>> m_rules;

public:
    OliSyntaxValidator() {
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

        bool inMultilineString = false;
        wchar_t activeQuote = L'\0';

        while (std::getline(ss, line)) {
            currentLineNum++;
            std::wstring cleanLine = trim(line);

            bool lineWasInMultiline = inMultilineString;

            // 1. Scanăm ghilimelele din linia curentă pentru starea multiline
            for (size_t i = 0; i < line.length(); ++i) {
                wchar_t c = line[i];
                if (c == L'\\') { i++; continue; }
                if (c == L'#' && activeQuote == L'\0') break;

                if (c == L'"' || c == L'\'') {
                    if (activeQuote == L'\0') {
                        activeQuote = c;
                    }
                    else if (activeQuote == c) {
                        activeQuote = L'\0';
                    }
                }
            }

            bool lineEndsInMultiline = (activeQuote != L'\0');

            // 2. Rulăm StringQuotesRule pe TOATE liniile pentru a-i menține starea internă sincronizată
            for (const auto& rule : m_rules) {
                if (dynamic_cast<StringQuotesRule*>(rule.get())) {
                    ShellCommand dummySC;
                    rule->check(dummySC, currentLineNum, line, errors);
                }
            }

            // 3. Dacă linia este conținut din interiorul unui string multiline (ex: C++ CUDA),
            // ignorăm doar regulile de structură Oli
            if (lineWasInMultiline) {
                inMultilineString = lineEndsInMultiline;
                continue;
            }

            inMultilineString = lineEndsInMultiline;

            if (cleanLine.empty() || cleanLine[0] == L'#') continue;

            // 4. Executăm regulile de structură Oli pentru liniile de cod normale
            std::vector<std::wstring> subCommands = splitBySemicolon(cleanLine);

            for (const auto& subCmdStr : subCommands) {
                std::wstring trimmedCmd = trim(subCmdStr);
                if (trimmedCmd.empty()) continue;

                ShellCommand sc = vOliCommandParser::parse(trimmedCmd);

                for (const auto& rule : m_rules) {
                    if (!dynamic_cast<StringQuotesRule*>(rule.get())) {
                        rule->check(sc, currentLineNum, line, errors);
                    }
                }
            }
        }

        // Faza finală (verificare globală la sfârșit de fișier)
        for (const auto& rule : m_rules) {
            rule->finalize(errors);
        }

        for (const auto& err : errors) {
            if (err.level == DiagnosticLevel::OLI_ERROR) {
                return false;
            }
        }

        return true;
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