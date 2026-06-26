#pragma once
#include <string>
#include <vector>
#include <memory>
#include "OliCommandParser.hpp" // Pentru ShellCommand

enum class DiagnosticLevel {
    OLI_ERROR,   // 🔥 Protejat de macro-ul din windows.h
    OLI_WARNING,
    OLI_NOTICE
};

struct SyntaxError {
    DiagnosticLevel level; // Noul membru pentru gradul de alertă
    std::wstring message;
    int lineNumber;
    std::wstring rawLine;
};

// Interfața abstractă pentru o regulă de sintaxă
class ISyntaxRule {
public:
    virtual ~ISyntaxRule() = default;

    // Returnează false dacă regula a fost încălcată și adaugă eroarea în vector
    virtual bool check(const ShellCommand& sc, int lineNum, const std::wstring& rawLine, std::vector<SyntaxError>& errors) = 0;
    virtual void finalize(std::vector<SyntaxError>&) {}
};