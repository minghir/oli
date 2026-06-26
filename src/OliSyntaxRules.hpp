#pragma once
#include "OliSyntaxChecker.hpp"
#include <algorithm>
#include <vector>
#include <cwctype>


// ❌ Regula 1: Validează structura comenzii SET
class SetCommandRule : public ISyntaxRule {
public:
    bool check(const ShellCommand& sc, int lineNum, const std::wstring& rawLine, std::vector<SyntaxError>& errors) override {
        std::wstring cmdUpper = sc.name;
        std::transform(cmdUpper.begin(), cmdUpper.end(), cmdUpper.begin(), ::toupper);

        if (cmdUpper != L"SET") return true;

        if (sc.args.empty()) {
            errors.push_back({ DiagnosticLevel::OLI_ERROR, L"'SET' command expects arguments.", lineNum, rawLine });
            return false;
        }

        // ❌ EROARE CRITICĂ: Atribuirea în lanț prăbușește parserul de expresii
        int equalCount = 0;
        for (const auto& arg : sc.args) {
            if (arg == L"=") equalCount++;
        }
        if (equalCount > 1) {
            errors.push_back({ DiagnosticLevel::OLI_ERROR, L"Multiple assignment in chain (X = Y = Z) is not supported.", lineNum, rawLine });
            return false;
        }

        // ℹ️ NOTICE: O simplă atenționare de stil, nu blochează rularea
        if (sc.args[0][0] == L'@') {
            errors.push_back({ DiagnosticLevel::OLI_NOTICE, L"Explicit global variable (@) used. Ensure it is defined in the global scope.", lineNum, rawLine });
        }

        return true;
    }
};

// ❌ Regula 2: Verifică comenzi necunoscute sau markeri orfani
class StructureValidatorRule : public ISyntaxRule {
public:
    bool check(const ShellCommand& sc, [[maybe_unused]] int lineNum, [[maybe_unused]] const std::wstring& rawLine,[[maybe_unused]] std::vector<SyntaxError>& errors) override {
        std::wstring cmdUpper = sc.name;
        std::transform(cmdUpper.begin(), cmdUpper.end(), cmdUpper.begin(), ::toupper);

        // Dacă o linie conține doar un marker de sfârșit fără context
        if (cmdUpper == L"ENDIF" || cmdUpper == L"ENDWHILE" || cmdUpper == L"ENDFOR" || cmdUpper == L"ENDFUNC") {
            // Această verificare simplă poate fi extinsă cu o stivă completă de nesting
            return true;
        }

        return true;
    }
};




// ⚠️ Regula: Avertizează când se folosesc identificatori fără prefix ($ sau @), dar ignoră comentariile, punctuația și funcțiile
class StrictVariableSyntaxRule : public ISyntaxRule {
private:
    bool isStructuralToken(std::wstring token, bool isNextTokenOpenParen) {
        std::wstring upper = token;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

        if (upper == L"(" || upper == L")" || upper == L"[" || upper == L"]" || upper == L",") {
            return true;
        }

        // 🌟 FIX: Ignorăm toate cuvintele cheie structurale ale limbajului care pot apărea inline
        if (upper == L"TO" || upper == L"DO" || upper == L"THEN" || upper == L"AS" || upper == L"STEP" ||
            upper == L"SET" || upper == L"IF" || upper == L"ENDIF" || upper == L"WHILE" || upper == L"ENDWHILE" ||
            upper == L"FOR" || upper == L"ENDFOR" || upper == L"FUNC" || upper == L"ENDFUNC" || upper == L"RETURN")
        {
            return true;
        }

        if (isNextTokenOpenParen) {
            return true;
        }

        return false;
    }

    void validateToken(const std::wstring& token, int lineNum, const std::wstring& rawLine, std::vector<SyntaxError>& errors) {
        if (token.empty()) return;

        wchar_t firstChar = token[0];

        if (firstChar != L'$' && firstChar != L'@' && firstChar != L'"' &&
            !std::iswdigit(firstChar) && firstChar != L'-' &&
            token != L"true" && token != L"false" && token != L"null" && token != L"NULL")
        {
            if (token.find_first_of(L"+-*/%^=<>&|.") == std::wstring::npos) {
                errors.push_back({
                    DiagnosticLevel::OLI_WARNING,
                    L"Identifier '" + token + L"' does not have a prefix ($ or @). It will be treated as an implicit global variable.",
                    lineNum,
                    rawLine
                    });
            }
        }
    }

public:
    bool check(const ShellCommand& sc, int lineNum, const std::wstring& rawLine, std::vector<SyntaxError>& errors) override {
        std::wstring cleanLine = trim(rawLine);
        if (!cleanLine.empty() && cleanLine[0] == L'#') {
            return true;
        }

        std::wstring cmdUpper = sc.name;
        std::transform(cmdUpper.begin(), cmdUpper.end(), cmdUpper.begin(), ::toupper);

        // 🌟 FIX: Ignorăm complet comenzile CONFIG și PLUGIN, parametrii lor sunt nume administrative libere
        if (cmdUpper == L"CONFIG" || cmdUpper == L"PLUGIN") {
            return true;
        }

        // PASUL 1: Scanăm argumentele
        for (size_t i = 0; i < sc.args.size(); ++i) {
            std::wstring token = sc.args[i];

            if (token == L"#" || (!token.empty() && token[0] == L'#')) {
                break;
            }

            bool nextIsParen = (i + 1 < sc.args.size() && sc.args[i + 1] == L"(");

            if (isStructuralToken(token, nextIsParen)) {
                continue;
            }

            validateToken(token, lineNum, rawLine, errors);
        }

        // PASUL 2: Scanăm stilul liber de atribuire (a = 10)
        if (!sc.args.empty() && sc.args[0] == L"=") {
            if (sc.name != L"#" && (sc.name.empty() || sc.name[0] != L'#')) {
                validateToken(sc.name, lineNum, rawLine, errors);
            }
        }

        return true;
    }
};


// ❌ EROARE CRITICĂ: Validează BREAK și CONTINUE în funcție de ierarhia de bucle și switch-uri
class LoopContextRule : public ISyntaxRule {
private:
    // Stivă internă care reține tipul blocului în care ne aflăm: L"LOOP" sau L"SWITCH"
    std::vector<std::wstring> m_contextStack;

public:
    bool check(const ShellCommand& sc, int lineNum, const std::wstring& rawLine, std::vector<SyntaxError>& errors) override {
        std::wstring cmdUpper = sc.name;
        std::transform(cmdUpper.begin(), cmdUpper.end(), cmdUpper.begin(), ::toupper);

        // 1. Intrăm într-o buclă -> adăugăm contextul pe stivă
        if (cmdUpper == L"WHILE" || cmdUpper == L"FOR" || cmdUpper == L"CYCLE" || cmdUpper == L"REPEAT") {
            m_contextStack.push_back(L"LOOP");
            return true;
        }

        // 2. Intrăm într-un SWITCH -> adăugăm contextul pe stivă
        if (cmdUpper == L"SWITCH") {
            m_contextStack.push_back(L"SWITCH");
            return true;
        }

        // 3. Ieșim dintr-o buclă -> scoatem de pe stivă
        if (cmdUpper == L"ENDWHILE" || cmdUpper == L"ENDFOR" || cmdUpper == L"ENDCYCLE" || cmdUpper == L"ENDREPEAT") {
            if (!m_contextStack.empty() && m_contextStack.back() == L"LOOP") {
                m_contextStack.pop_back();
            }
            return true;
        }

        // 4. Ieșim din SWITCH -> scoatem de pe stivă
        if (cmdUpper == L"ENDSWITCH") {
            if (!m_contextStack.empty() && m_contextStack.back() == L"SWITCH") {
                m_contextStack.pop_back();
            }
            return true;
        }

        // 5. Validare BREAK: Trebuie să fim ori într-un LOOP, ori într-un SWITCH (stiva nu e goală)
        if (cmdUpper == L"BREAK") {
            if (m_contextStack.empty()) {
                errors.push_back({
                    DiagnosticLevel::OLI_ERROR,
                    L"Instruction 'BREAK' cannot be used outside a loop or a SWITCH block.",
                    lineNum,
                    rawLine
                    });
                return false;
            }
        }

        // 6. Validare CONTINUE: Este valid doar dacă există CEL PUȚIN un context de LOOP în toată stiva
        if (cmdUpper == L"CONTINUE") {
            bool hasLoopContext = false;
            for (const auto& ctx : m_contextStack) {
                if (ctx == L"LOOP") {
                    hasLoopContext = true;
                    break;
                }
            }

            if (!hasLoopContext) {
                errors.push_back({
                    DiagnosticLevel::OLI_ERROR,
                    L"Instruction 'CONTINUE' can only be used inside a loop (WHILE, FOR, CYCLE, REPEAT).",
                    lineNum,
                    rawLine
                    });
                return false;
            }
        }

        return true;
    }
};




// Structură internă pentru a ține minte unde s-a deschis fiecare bloc
struct BlockOpenInfo {
    std::wstring type;     // L"IF", L"WHILE", L"FUNC" etc.
    int lineNumber;        // Linia exactă de unde a pornit
    std::wstring rawLine;  // Codul original de la acea linie
};

class NestingBalanceRule : public ISyntaxRule {
private:
    std::vector<BlockOpenInfo> m_openedBlocks;

    std::wstring getExpectedStartToken(const std::wstring& endCmd) {
        if (endCmd == L"ENDIF")     return L"IF";
        if (endCmd == L"ENDWHILE")   return L"WHILE";
        if (endCmd == L"ENDFOR")     return L"FOR";
        if (endCmd == L"ENDFUNC")    return L"FUNC";
        if (endCmd == L"ENDPROC")    return L"PROC";
        if (endCmd == L"ENDCYCLE")   return L"CYCLE";
        if (endCmd == L"ENDSWITCH")  return L"SWITCH";
        if (endCmd == L"ENDREPEAT")  return L"REPEAT";
        return L"";
    }

public:
    bool check(const ShellCommand& sc, int lineNum, const std::wstring& rawLine, std::vector<SyntaxError>& errors) override {
        std::wstring cmdUpper = sc.name;
        std::transform(cmdUpper.begin(), cmdUpper.end(), cmdUpper.begin(), ::toupper);

        // 1. Dacă întâlnim un început de bloc structural (IF, WHILE, etc.)
        if (cmdUpper == L"IF" || cmdUpper == L"WHILE" || cmdUpper == L"FOR" ||
            cmdUpper == L"FUNC" || cmdUpper == L"PROC" || cmdUpper == L"CYCLE" ||
            cmdUpper == L"SWITCH" || cmdUpper == L"REPEAT")
        {
            // 🌟 FIX CRITIC: Verificăm dacă blocul se închide inline pe aceeași linie (ex: ... endif)
            std::wstring expectedEnd = L"END" + cmdUpper;
            bool closedOnSameLine = false;

            for (const auto& arg : sc.args) {
                std::wstring argUpper = arg;
                std::transform(argUpper.begin(), argUpper.end(), argUpper.begin(), ::toupper);
                if (argUpper == expectedEnd) {
                    closedOnSameLine = true;
                    break;
                }
            }

            // Îl punem pe stivă DOAR dacă nu s-a închis deja pe aceeași linie!
            if (!closedOnSameLine) {
                m_openedBlocks.push_back({ cmdUpper, lineNum, rawLine });
            }
            return true;
        }

        // 2. Dacă întâlnim un sfârșit de bloc structural pe o linie separată
        std::wstring expectedStart = getExpectedStartToken(cmdUpper);
        if (!expectedStart.empty()) {

            if (m_openedBlocks.empty()) {
                errors.push_back({
                    DiagnosticLevel::OLI_ERROR,
                    L"Instruction '" + cmdUpper + L"' does not have a corresponding opening.",
                    lineNum,
                    rawLine
                    });
                return false;
            }

            if (m_openedBlocks.back().type != expectedStart) {
                errors.push_back({
                    DiagnosticLevel::OLI_ERROR,
                    L"Invalid crossed syntax: '" + cmdUpper + L"' tries to close a block of type '" +
                    m_openedBlocks.back().type + L"' opened on line " + std::to_wstring(m_openedBlocks.back().lineNumber) + L".",
                    lineNum,
                    rawLine
                    });
                return false;
            }

            m_openedBlocks.pop_back();
        }

        return true;
    }

    void finalize(std::vector<SyntaxError>& errors) override {
        while (!m_openedBlocks.empty()) {
            BlockOpenInfo unclosed = m_openedBlocks.back();
            m_openedBlocks.pop_back();

            errors.push_back({
                DiagnosticLevel::OLI_ERROR,
                L"Control structure '" + unclosed.type + L"' opened on line " +
                std::to_wstring(unclosed.lineNumber) + L" was not closed (missing 'END" + unclosed.type + L"').",
                unclosed.lineNumber,
                unclosed.rawLine
                });
        }
    }
};


// ❌ EROARE CRITICĂ: Prinde atribuiri libere sau explicite cu LHS invalid (ex: 10 = $b sau set 10 = $b)
class InvalidLHSRule : public ISyntaxRule {
public:
    bool check(const ShellCommand& sc, int lineNum, const std::wstring& rawLine, std::vector<SyntaxError>& errors) override {
        std::wstring lhsToken = L"";
        bool foundEquals = false;

        // Cazul 1: Stilul liber direct (ex: 10 = $b)
        // În acest caz, sc.name este chiar valoarea stângă, iar primul argument este "="
        if (!sc.args.empty() && sc.args[0] == L"=") {
            lhsToken = sc.name;
            foundEquals = true;
        }
        // Cazul 2: Stilul explicit cu comandă (ex: set 10 = $b) sau expresie în interiorul argumentelor
        else {
            auto it = std::find(sc.args.begin(), sc.args.end(), L"=");
            if (it != sc.args.end()) {
                foundEquals = true;
                if (it == sc.args.begin()) {
                    lhsToken = L""; // Nu există nimic înainte de "=" în interiorul argumentelor
                }
                else {
                    lhsToken = *(it - 1);
                }
            }
        }

        // Dacă nu există niciun operator de atribuire '=', linia e validă pentru alte reguli
        if (!foundEquals) return true;

        // Dacă am găsit '=', dar destinația (LHS) lipsește complet (ex: 'set = 10')
        if (lhsToken.empty()) {
            std::wstring cmdUpper = sc.name;
            std::transform(cmdUpper.begin(), cmdUpper.end(), cmdUpper.begin(), ::toupper);
            if (cmdUpper == L"SET") {
                errors.push_back({
                    DiagnosticLevel::OLI_ERROR,
                    L"Missing data for assignment destination (LHS missing before '=').",
                    lineNum,
                    rawLine
                    });
                return false;
            }
            return true;
        }

        // Analizăm primul caracter al destinației (LHS)
        wchar_t firstChar = lhsToken[0];

        // Definim ce tipuri de literali sunt complet interziși la scriere
        bool isNumber = std::iswdigit(firstChar) || (lhsToken.size() > 1 && firstChar == L'-' && std::iswdigit(lhsToken[1]));
        bool isString = (lhsToken.size() >= 2 && lhsToken.front() == L'\"' && lhsToken.back() == L'\"');
        bool isBooleanOrNull = (lhsToken == L"true" || lhsToken == L"false" || lhsToken == L"null" || lhsToken == L"NULL");

        if (isNumber || isString || isBooleanOrNull) {
            errors.push_back({
                DiagnosticLevel::OLI_ERROR, // ❌ Blocăm compilarea!
                L"Invalid destination for assignment (LHS invalid). You cannot modify the value of a constant or a literal of type '" + lhsToken + L"'.",
                lineNum,
                rawLine
                });
            return false;
        }

        return true;
    }
};

// ❌ EROARE CRITICĂ: Prinde ghilimelele neînchise la finalul fișierului
class StringQuotesRule : public ISyntaxRule {
private:
    bool m_inString = false;       // Flag care ne spune dacă suntem în interiorul unui string literal
    int m_openedAtLine = -1;       // Reține linia exactă unde s-au deschis ghilimelele
    std::wstring m_openedRawLine;  // Reține codul original al liniei de deschidere

public:
    bool check([[maybe_unused]] const ShellCommand& sc,[[maybe_unused]] int lineNum,[[maybe_unused]] const std::wstring& rawLine, [[maybe_unused]]std::vector<SyntaxError>& errors) override {

        // Scanăm linia caracter cu caracter pentru a gestiona corect stările
        for (size_t i = 0; i < rawLine.length(); ++i) {
            wchar_t c = rawLine[i];

            // 1. SCUTUL ANTI-ESCAPE: Dacă vedem un backslash, sărim peste el și peste următorul caracter
            if (c == L'\\') {
                i++;
                continue;
            }

            // 2. GESTIONARE COMENTARII: Dacă vedem '#' și NU suntem în interiorul unui string, 
            // ignorăm restul liniei (ghilimelele din comentarii nu se contorizează)
            if (c == L'#' && !m_inString) {
                break;
            }

            // 3. MAȘINA DE STĂRI PENTRU GHILIMELE
            if (c == L'"') {
                m_inString = !m_inString;

                if (m_inString) {
                    // S-a deschis un string nou, salvăm coordonatele pentru diagnosticare
                    m_openedAtLine = lineNum;
                    m_openedRawLine = rawLine;
                }
                else {
                    // S-a închis stringul corect, resetăm markerii
                    m_openedAtLine = -1;
                    m_openedRawLine = L"";
                }
            }
        }

        return true;
    }

    // 4. VERIFICAREA FINALĂ: Apelată automat la sfârșitul fișierului
    void finalize(std::vector<SyntaxError>& errors) override {
        // Dacă fișierul s-a terminat și m_inString este încă true, avem o problemă!
        if (m_inString && m_openedAtLine != -1) {
            errors.push_back({
                DiagnosticLevel::OLI_ERROR, // ❌ Eroare critică (Blocantă)
                L"Unquoted string. The string opened on line " + std::to_wstring(m_openedAtLine) + L" was not closed until the end of the file.",
                m_openedAtLine,
                m_openedRawLine
                });
        }
    }
};