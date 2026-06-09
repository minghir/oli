#pragma once

#include "OliSyntaxChecker.hpp"
#include "OliSyntaxRules.hpp"

class OliSyntaxValidator {
private:
    std::vector<std::unique_ptr<ISyntaxRule>> m_rules;

public:
    OliSyntaxValidator() {
        // Înregistrăm regulile în pipeline
        /*
        m_rules.push_back(std::make_unique<SetCommandRule>());
        m_rules.push_back(std::make_unique<StructureValidatorRule>());
        m_rules.push_back(std::make_unique<StrictVariableSyntaxRule>());
        m_rules.push_back(std::make_unique<LoopContextRule>());
        m_rules.push_back(std::make_unique<NestingBalanceRule>());
		m_rules.push_back(std::make_unique<InvalidLHSRule>());
        m_rules.push_back(std::make_unique<StringQuotesRule>());
        */

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