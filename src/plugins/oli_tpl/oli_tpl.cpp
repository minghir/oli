#include "../../OliEngine.hpp"
#include "../../vData.hpp"
#include "../../ConsoleManager.hpp"
#include "../../OliKeyWords.hpp"

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>
#include <memory>
#include <filesystem>
#include <cctype>
#include <cwctype>
#include <algorithm>
#include <cmath>

#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#include <windows.h>
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#include <clocale>
#endif

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;
using Scope = std::map<std::wstring, vData>;

// ============================================================================
// CONVERSIE UTF-8 ȘI FIȘIERE
// ============================================================================

static std::wstring utf8ToWstring(const std::string& str) {
    if (str.empty()) return L"";
#if defined(_WIN32) || defined(_WIN64)
    int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0);
    std::wstring wstr(count, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &wstr[0], count);
    return wstr;
#else
    std::mbstate_t state = std::mbstate_t();
    const char* src = str.c_str();
    size_t len = 1 + std::mbsrtowcs(nullptr, &src, 0, &state);
    if (len == 0 || len == static_cast<size_t>(-1)) return L"";
    std::vector<wchar_t> buf(len);
    std::mbsrtowcs(buf.data(), &src, len, &state);
    return std::wstring(buf.data());
#endif
}

static std::string wstringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
#if defined(_WIN32) || defined(_WIN64)
    int count = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
    std::string str(count, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &str[0], count, NULL, NULL);
    return str;
#else
    std::mbstate_t state = std::mbstate_t();
    const wchar_t* src = wstr.c_str();
    size_t len = 1 + std::wcsrtombs(nullptr, &src, 0, &state);
    if (len == 0 || len == static_cast<size_t>(-1)) return "";
    std::vector<char> buf(len);
    std::wcsrtombs(buf.data(), &src, len, &state);
    return std::string(buf.data());
#endif
}

static std::wstring readTextFileUtf8(const std::wstring& filePath) {
    std::string pathUtf8 = wstringToUtf8(filePath);
    std::ifstream file(pathUtf8, std::ios::binary);
    if (!file.is_open()) return L"";

    std::stringstream ss;
    ss << file.rdbuf();
    return utf8ToWstring(ss.str());
}

static bool writeTextFileUtf8(const std::wstring& filePath, const std::wstring& content) {
    std::string pathUtf8 = wstringToUtf8(filePath);
    try {
        std::filesystem::path p(pathUtf8);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        std::ofstream file(p, std::ios::binary);
        if (!file.is_open()) return false;

        std::string contentUtf8 = wstringToUtf8(content);
        file.write(contentUtf8.c_str(), contentUtf8.size());
        return file.good();
    } catch (...) {
        return false;
    }
}

// ============================================================================
// ESCAPARE RTF SI TRIMMING
// ============================================================================

static std::wstring trimWString(const std::wstring& str) {
    size_t first = str.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return L"";
    size_t last = str.find_last_not_of(L" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::wstring escapeRTFString(const std::wstring& input) {
    std::wstringstream ss;
    for (wchar_t ch : input) {
        switch (ch) {
            case L'\\': ss << L"\\\\"; break;
            case L'{':  ss << L"\\{"; break;
            case L'}':  ss << L"\\}"; break;
            default:
                if (ch > 127) {
                    short code = static_cast<short>(ch);
                    ss << L"\\u" << code << L"?";
                } else {
                    ss << ch;
                }
                break;
        }
    }
    return ss.str();
}


// Filter: number_format(num, decimals, dec_sep, thousand_sep)
static std::wstring filterNumberFormat(const std::wstring& input, const std::vector<std::wstring>& args) {
    if (input.empty()) return L"";

    double num = 0.0;
    try {
        num = std::stod(input);
    } catch (...) {
        return input; // Dacă nu e număr, returnăm textul brut
    }

    int decimals = (args.size() >= 1) ? std::stoi(args[0]) : 2;
    wchar_t decSep = (args.size() >= 2 && !args[1].empty()) ? args[1][0] : L',';
    wchar_t thousandSep = (args.size() >= 3 && !args[2].empty()) ? args[2][0] : L'.';

    std::wstringstream ss;
    ss << std::fixed << std::setprecision(decimals) << num;
    std::wstring str = ss.str();

    size_t dotPos = str.find(L'.');
    std::wstring intPart = (dotPos != std::wstring::npos) ? str.substr(0, dotPos) : str;
    std::wstring decPart = (dotPos != std::wstring::npos) ? str.substr(dotPos + 1) : L"";

    // Formatare separatori de mii
    std::wstring formattedInt;
    int count = 0;
    bool isNegative = (!intPart.empty() && intPart[0] == L'-');
    size_t startIdx = isNegative ? 1 : 0;

    for (int i = (int)intPart.length() - 1; i >= (int)startIdx; --i) {
        if (count > 0 && count % 3 == 0) {
            formattedInt += thousandSep;
        }
        formattedInt += intPart[i];
        count++;
    }
    if (isNegative) formattedInt += L'-';
    std::reverse(formattedInt.begin(), formattedInt.end());

    if (decimals > 0) {
        return formattedInt + decSep + decPart;
    }
    return formattedInt;
}

// Filter: date_format(ISO_date_string, output_format)
// Suportă format ISO "YYYY-MM-DD" sau "YYYY-MM-DD HH:MM:SS" -> "d.m.Y"
static std::wstring filterDateFormat(const std::wstring& input, const std::vector<std::wstring>& args) {
    if (input.empty()) return L"";
    std::wstring fmt = (args.size() >= 1) ? args[0] : L"d.m.Y";

    int y = 0, m = 0, d = 0, H = 0, i = 0, s = 0;
    if (swscanf(input.c_str(), L"%d-%d-%d %d:%d:%d", &y, &m, &d, &H, &i, &s) < 3) {
        if (swscanf(input.c_str(), L"%d-%d-%d", &y, &m, &d) < 3) {
            return input; // Format nedetectat
        }
    }

    wchar_t buf[128];
    std::wstring res = fmt;

    auto replaceTag = [&](const std::wstring& tag, int val, int width) {
        swprintf(buf, 128, (width == 4) ? L"%04d" : L"%02d", val);
        size_t p;
        while ((p = res.find(tag)) != std::wstring::npos) {
            res.replace(p, tag.length(), buf);
        }
    };

    replaceTag(L"Y", y, 4);
    replaceTag(L"m", m, 2);
    replaceTag(L"d", d, 2);
    replaceTag(L"H", H, 2);
    replaceTag(L"i", i, 2);
    replaceTag(L"s", s, 2);

    return res;
}


struct FilterCall {
    std::wstring name;
    std::vector<std::wstring> args;
};

// Parser pentru expresia de filtre (ex: "number_format:2:',':'.'")
static std::wstring applyFilterChain(std::wstring valStr, const std::wstring& rawDirective) {
    size_t pipePos = rawDirective.find(L"|");
    if (pipePos == std::wstring::npos) return valStr;

    std::wstring filtersPart = rawDirective.substr(pipePos + 1);
    std::wstringstream ss(filtersPart);
    std::wstring filterSegment;

    while (std::getline(ss, filterSegment, L'|')) {
        filterSegment = trimWString(filterSegment);
        if (filterSegment.empty()) continue;

        // Extragere nume filtru și argumente delimitate prin ':'
        std::wstringstream fss(filterSegment);
        std::wstring fName, arg;
        std::vector<std::wstring> fArgs;

        std::getline(fss, fName, L':');
        fName = trimWString(fName);

        while (std::getline(fss, arg, L':')) {
            arg = trimWString(arg);
            // Curățare ghilimele din argumente
            if (arg.length() >= 2 && (arg.front() == L'"' || arg.front() == L'\'')) {
                arg = arg.substr(1, arg.length() - 2);
            }
            fArgs.push_back(arg);
        }

        // Execuție filtru
        if (fName == L"number_format") {
            valStr = filterNumberFormat(valStr, fArgs);
        } else if (fName == L"date_format") {
            valStr = filterDateFormat(valStr, fArgs);
        } else if (fName == L"upper") {
            for (auto& c : valStr) c = std::towupper(c);
        } else if (fName == L"lower") {
            for (auto& c : valStr) c = std::towlower(c);
        }
    }

    return valStr;
}


// ============================================================================
// EVALUATOR ȘI LOOKUP ÎN CONTEXTUL DE DATE
// ============================================================================
static vData resolveVar(
    const std::wstring& rawExpr, 
    const std::vector<Scope>& scopeStack, 
    const PluginRegistry& registry = {}
) {
    std::wstring expr = trimWString(rawExpr);
    if (expr.empty()) return vData(L"");

    // Helper intern pentru conversia sigură din vData (String / Int / Double) în double
    auto vDataToDouble = [](const vData& v) -> double {
        if (v.isNumber()) return v.toDouble();
        if (v.isString()) {
            try {
                std::wstring s = trimWString(v.toWString());
                if (!s.empty()) return std::stod(s);
            } catch (...) {}
        }
        return 0.0;
    };

    // ========================================================================
    // 1. CURĂȚARE FILTRU DE FORMATARE (ex: $p.mtow / 1000 | rtf)
    // ========================================================================
    size_t pipePos = expr.find(L"|");
    if (pipePos != std::wstring::npos) {
        expr = trimWString(expr.substr(0, pipePos));
    }

    // ========================================================================
    // 2. ELIMINARE PARANTEZE EXTERIOARE REDUNDANTE (ex: ($p.mtow / 1000))
    // ========================================================================
    while (expr.length() >= 2 && expr.front() == L'(' && expr.back() == L')') {
        int depth = 0;
        bool wrapsAll = true;
        for (size_t i = 0; i < expr.length() - 1; ++i) {
            if (expr[i] == L'(') depth++;
            else if (expr[i] == L')') depth--;
            if (depth == 0) { wrapsAll = false; break; }
        }
        if (wrapsAll) {
            expr = trimWString(expr.substr(1, expr.length() - 2));
        } else {
            break;
        }
    }

    // ========================================================================
    // 3. EVALUARE EXPRESII MATEMATICE SIMPLE (+, -, *, /)
    // ========================================================================
    int lowestOpPos = -1;
    wchar_t lowestOp = 0;

    int parenDepth = 0;
    bool inQ = false;
    wchar_t qChar = 0;

    // Prioritate scăzută: Adunare (+) și Scădere (-)
    for (int i = (int)expr.length() - 1; i >= 0; --i) {
        wchar_t ch = expr[i];
        if ((ch == L'"' || ch == L'\'') && (qChar == 0 || qChar == ch)) {
            inQ = !inQ;
            qChar = inQ ? ch : 0;
        } else if (!inQ && ch == L')') {
            parenDepth++;
        } else if (!inQ && ch == L'(') {
            parenDepth--;
        } else if (!inQ && parenDepth == 0) {
            if (ch == L'+' || ch == L'-') {
                if (ch == L'-') {
                    if (i == 0) continue;
                    wchar_t prev = expr[i - 1];
                    if (prev == L'+' || prev == L'-' || prev == L'*' || prev == L'/' || prev == L'(' || prev == L',') continue;
                }
                lowestOpPos = i;
                lowestOp = ch;
                break;
            }
        }
    }

    // Prioritate ridicată: Înmulțire (*) și Împărțire (/)
    if (lowestOpPos == -1) {
        parenDepth = 0;
        inQ = false;
        qChar = 0;
        for (int i = (int)expr.length() - 1; i >= 0; --i) {
            wchar_t ch = expr[i];
            if ((ch == L'"' || ch == L'\'') && (qChar == 0 || qChar == ch)) {
                inQ = !inQ;
                qChar = inQ ? ch : 0;
            } else if (!inQ && ch == L')') {
                parenDepth++;
            } else if (!inQ && ch == L'(') {
                parenDepth--;
            } else if (!inQ && parenDepth == 0) {
                if (ch == L'*' || ch == L'/') {
                    lowestOpPos = i;
                    lowestOp = ch;
                    break;
                }
            }
        }
    }

    if (lowestOpPos != -1) {
        std::wstring leftStr = trimWString(expr.substr(0, lowestOpPos));
        std::wstring rightStr = trimWString(expr.substr(lowestOpPos + 1));

        vData leftVal = resolveVar(leftStr, scopeStack, registry);
        vData rightVal = resolveVar(rightStr, scopeStack, registry);

        if (lowestOp == L'+' && leftVal.isString() && !rightVal.isNumber()) {
            return vData(leftVal.toWString() + rightVal.toWString());
        }

        double lNum = vDataToDouble(leftVal);
        double rNum = vDataToDouble(rightVal);

        if (lowestOp == L'+') return vData(lNum + rNum);
        if (lowestOp == L'-') return vData(lNum - rNum);
        if (lowestOp == L'*') return vData(lNum * rNum);
        if (lowestOp == L'/') {
            if (rNum == 0.0) return vData(0LL);
            double res = lNum / rNum;
            if (std::floor(res) == res) return vData((long long)res); // Afișează curat "54" în loc de "54.000000"
            return vData(res);
        }
    }

    // ========================================================================
    // 4. PARSARE ȘI EVALUARE APELURI DE FUNCȚII: FUNCTIE(arg1, arg2, ...)
    // ========================================================================
    size_t openParen = expr.find(L'(');
    size_t closeParen = expr.rfind(L')');

    if (openParen != std::wstring::npos && closeParen != std::wstring::npos && closeParen > openParen) {
        std::wstring funcName = trimWString(expr.substr(0, openParen));
        std::wstring uFuncName = funcName;
        for (auto& c : uFuncName) c = std::towupper(c);

        vOliKeyWords::populateNativeFunctions();

        bool isFunc = vOliKeyWords::isNativeFunction(uFuncName) || 
                      (registry.find(uFuncName) != registry.end()) ||
                      (uFuncName == L"TRIM" || uFuncName == L"UPPER" || uFuncName == L"LOWER" || 
                       uFuncName == L"STR" || uFuncName == L"LEN" || uFuncName == L"LENGTH" || 
                       uFuncName == L"SUBSTR" || uFuncName == L"SUBSTRING" || uFuncName == L"COALESCE" || 
                       uFuncName == L"NVL" || uFuncName == L"IFNULL" || uFuncName == L"EVAL" || uFuncName == L"ROUND");

        if (isFunc) {
            std::wstring innerArgs = trimWString(expr.substr(openParen + 1, closeParen - openParen - 1));
            std::vector<vData> evalArgs;

            if (!innerArgs.empty()) {
                std::wstring currentArg;
                bool inQArg = false;
                wchar_t qCharArg = 0;
                int pDepthArg = 0;

                for (size_t idx = 0; idx < innerArgs.length(); ++idx) {
                    wchar_t ch = innerArgs[idx];
                    if ((ch == L'"' || ch == L'\'') && (qCharArg == 0 || qCharArg == ch)) {
                        inQArg = !inQArg;
                        qCharArg = inQArg ? ch : 0;
                        currentArg += ch;
                    } else if (!inQArg && ch == L'(') {
                        pDepthArg++;
                        currentArg += ch;
                    } else if (!inQArg && ch == L')') {
                        pDepthArg--;
                        currentArg += ch;
                    } else if (!inQArg && pDepthArg == 0 && ch == L',') {
                        currentArg = trimWString(currentArg);
                        if (!currentArg.empty()) {
                            if ((currentArg.front() == L'"' || currentArg.front() == L'\'') && currentArg.back() == currentArg.front()) {
                                evalArgs.push_back(vData(currentArg.substr(1, currentArg.length() - 2)));
                            } else {
                                evalArgs.push_back(resolveVar(currentArg, scopeStack, registry));
                            }
                        } else {
                            evalArgs.push_back(vData(L""));
                        }
                        currentArg.clear();
                    } else {
                        currentArg += ch;
                    }
                }

                currentArg = trimWString(currentArg);
                if (!currentArg.empty()) {
                    if ((currentArg.front() == L'"' || currentArg.front() == L'\'') && currentArg.back() == currentArg.front()) {
                        evalArgs.push_back(vData(currentArg.substr(1, currentArg.length() - 2)));
                    } else {
                        evalArgs.push_back(resolveVar(currentArg, scopeStack, registry));
                    }
                }
            }

            auto regIt = registry.find(uFuncName);
            if (regIt != registry.end()) {
                return regIt->second(evalArgs);
            }

            if (uFuncName == L"EVAL" && !evalArgs.empty()) {
                return evalArgs[0];
            }
            if (uFuncName == L"ROUND" && !evalArgs.empty()) {
                double val = vDataToDouble(evalArgs[0]);
                return vData(std::round(val));
            }
            if (uFuncName == L"TRIM" && !evalArgs.empty()) {
                return vData(trimWString(evalArgs[0].toWString()));
            }
            if (uFuncName == L"UPPER" && !evalArgs.empty()) {
                std::wstring s = evalArgs[0].toWString();
                for (auto& c : s) c = std::towupper(c);
                return vData(s);
            }
            if (uFuncName == L"LOWER" && !evalArgs.empty()) {
                std::wstring s = evalArgs[0].toWString();
                for (auto& c : s) c = std::towlower(c);
                return vData(s);
            }
            if (uFuncName == L"STR" && !evalArgs.empty()) {
                return vData(evalArgs[0].toWString());
            }
            if ((uFuncName == L"LEN" || uFuncName == L"LENGTH") && !evalArgs.empty()) {
                return vData((long long)evalArgs[0].toWString().length());
            }
            if ((uFuncName == L"NVL" || uFuncName == L"IFNULL" || uFuncName == L"COALESCE") && evalArgs.size() >= 2) {
                std::wstring val = evalArgs[0].toWString();
                return val.empty() ? evalArgs[1] : evalArgs[0];
            }
            if ((uFuncName == L"SUBSTR" || uFuncName == L"SUBSTRING") && evalArgs.size() >= 2) {
                std::wstring s = evalArgs[0].toWString();
                size_t start = (size_t)evalArgs[1].toInt();
                size_t len = (evalArgs.size() >= 3) ? (size_t)evalArgs[2].toInt() : std::wstring::npos;
                if (start < s.length()) {
                    return vData(s.substr(start, len));
                }
                return vData(L"");
            }
        }
    }

    // ========================================================================
    // 5. REZOLVARE VARIABILE ($p.registration) SAU LITERALE NUMERICE / TEXT
    // ========================================================================
    std::vector<std::wstring> tokens;
    std::wstring curToken;
    bool inQuotes = false;

    for (size_t i = 0; i < expr.length(); ++i) {
        wchar_t ch = expr[i];
        if (ch == L'"' || ch == L'\'') {
            inQuotes = !inQuotes;
        } else if (!inQuotes && (ch == L'.' || ch == L'[' || ch == L']')) {
            if (!curToken.empty()) {
                tokens.push_back(curToken);
                curToken.clear();
            }
        } else {
            curToken += ch;
        }
    }
    if (!curToken.empty()) tokens.push_back(curToken);

    if (tokens.empty()) return vData(L"");

    std::wstring rootKey = trimWString(tokens[0]);

    if (rootKey.length() >= 2 && (rootKey.front() == L'"' || rootKey.front() == L'\'') && rootKey.back() == rootKey.front()) {
        return vData(rootKey.substr(1, rootKey.length() - 2));
    }

    vData current;
    bool found = false;

    for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
        const Scope& scope = *it;

        auto sIt = scope.find(rootKey);
        if (sIt != scope.end()) {
            current = sIt->second;
            found = true;
            break;
        }

        std::wstring altKey = (rootKey[0] == L'$') ? rootKey.substr(1) : (L"$" + rootKey);
        sIt = scope.find(altKey);
        if (sIt != scope.end()) {
            current = sIt->second;
            found = true;
            break;
        }
    }

    if (!found) {
        try {
            size_t idx = 0;
            if (rootKey.find(L'.') != std::wstring::npos) {
                double d = std::stod(rootKey, &idx);
                if (idx == rootKey.length()) return vData(d);
            } else {
                long long l = std::stoll(rootKey, &idx);
                if (idx == rootKey.length()) return vData(l);
            }
        } catch (...) {}
        return vData(L"");
    }

    for (size_t i = 1; i < tokens.size(); ++i) {
        std::wstring key = trimWString(tokens[i]);
        if (key.length() >= 2 && (key.front() == L'"' || key.front() == L'\'') && key.back() == key.front()) {
            key = key.substr(1, key.length() - 2);
        }

        if (current.isMap() && current.rawMap() != nullptr) {
            auto* m = current.rawMap();
            auto mIt = m->find(key);
            if (mIt == m->end()) {
                std::wstring altKey = (key[0] == L'$') ? key.substr(1) : (L"$" + key);
                mIt = m->find(altKey);
            }

            if (mIt != m->end()) {
                current = mIt->second;
            } else {
                return vData(L"");
            }
        } else if (current.isArray() && current.rawArray() != nullptr) {
            auto* arr = current.rawArray();
            try {
                size_t idx = std::stoul(key);
                if (idx < arr->size()) {
                    current = (*arr)[idx];
                } else {
                    return vData(L"");
                }
            } catch (...) {
                return vData(L"");
            }
        } else {
            return vData(L"");
        }
    }

    return current;
}

static bool evaluateCondition(const std::wstring& condExpr, const std::vector<Scope>& scopeStack) {
    std::wstring expr = trimWString(condExpr);
    if (expr.empty()) return false;

    std::vector<std::wstring> ops = { L"==", L"!=", L">=", L"<=", L">", L"<" };
    for (const auto& op : ops) {
        size_t pos = expr.find(op);
        if (pos != std::wstring::npos) {
            std::wstring leftStr = trimWString(expr.substr(0, pos));
            std::wstring rightStr = trimWString(expr.substr(pos + op.length()));

            vData leftVal = resolveVar(leftStr, scopeStack);
            vData rightVal;

            if (!rightStr.empty() && (rightStr.front() == L'"' || rightStr.front() == L'\'')) {
                rightVal = vData(rightStr.substr(1, rightStr.length() - 2));
            } else if (!rightStr.empty() && (std::iswdigit(rightStr.front()) || rightStr.front() == L'-')) {
                try {
                    rightVal = vData(std::stoll(rightStr));
                } catch (...) {
                    rightVal = vData(rightStr);
                }
            } else {
                rightVal = resolveVar(rightStr, scopeStack);
            }

            if (op == L"==") return leftVal.toWString() == rightVal.toWString();
            if (op == L"!=") return leftVal.toWString() != rightVal.toWString();

            double lNum = leftVal.isNumber() ? leftVal.toDouble() : 0.0;
            double rNum = rightVal.isNumber() ? rightVal.toDouble() : 0.0;

            if (op == L">") return lNum > rNum;
            if (op == L"<") return lNum < rNum;
            if (op == L">=") return lNum >= rNum;
            if (op == L"<=") return lNum <= rNum;
        }
    }

    vData val = resolveVar(expr, scopeStack);
    if (val.isBool()) return val.toBool();
    if (val.isInt()) return val.toInt() != 0;
    if (val.isFloat()) return val.toDouble() != 0.0;
    if (val.isString()) {
        std::wstring s = val.toWString();
        return !s.empty() && s != L"0" && s != L"false";
    }
    if (val.isArray() && val.rawArray() != nullptr) return !val.rawArray()->empty();
    if (val.isMap() && val.rawMap() != nullptr) return !val.rawMap()->empty();

    return false;
}

// ============================================================================
// PARSER SI EVALUATOR AST TEMPLATE
// ============================================================================

enum class NodeType { TEXT, VAR, CYCLE, IF_COND };

struct TplNode {
    NodeType type;
    std::wstring content;
    std::wstring arrayExpr;
    std::wstring itemVarName;
    std::vector<TplNode> children;
    std::vector<TplNode> elseChildren;
    bool isRaw = false;
    bool isRtf = false;
};

static std::vector<TplNode> parseTemplateTokens(
    const std::wstring& tpl,
    size_t& pos,
    const std::wstring& openTag,
    const std::wstring& closeTag,
    const std::wstring& stopTag1,
    const std::wstring& stopTag2,
    std::wstring& hitTag
) {
    std::vector<TplNode> nodes;
    size_t len = tpl.length();
    hitTag.clear();

    while (pos < len) {
        size_t startTag = tpl.find(openTag, pos);
        if (startTag == std::wstring::npos) {
            std::wstring text = tpl.substr(pos);
            if (!text.empty()) {
                nodes.push_back({ NodeType::TEXT, text });
            }
            pos = len;
            break;
        }

        if (startTag > pos) {
            std::wstring text = tpl.substr(pos, startTag - pos);
            nodes.push_back({ NodeType::TEXT, text });
        }

        size_t endTag = tpl.find(closeTag, startTag);
        if (endTag == std::wstring::npos) {
            nodes.push_back({ NodeType::TEXT, tpl.substr(startTag) });
            pos = len;
            break;
        }

        size_t contentStart = startTag + openTag.length();
        size_t contentLen = endTag - contentStart;
        std::wstring directive = trimWString(tpl.substr(contentStart, contentLen));

        pos = endTag + closeTag.length();

        if (directive.empty()) continue;

        if (!stopTag1.empty() && directive == stopTag1) {
            hitTag = stopTag1;
            return nodes;
        }
        if (!stopTag2.empty() && directive == stopTag2) {
            hitTag = stopTag2;
            return nodes;
        }

        if (directive.rfind(L"cycle ", 0) == 0) {
            TplNode cycleNode;
            cycleNode.type = NodeType::CYCLE;

            std::wstring args = trimWString(directive.substr(6));
            size_t asPos = args.find(L" as ");
            if (asPos != std::wstring::npos) {
                cycleNode.arrayExpr = trimWString(args.substr(0, asPos));
                cycleNode.itemVarName = trimWString(args.substr(asPos + 4));
            } else {
                cycleNode.arrayExpr = args;
                cycleNode.itemVarName = L"$item";
            }

            std::wstring dummyHit;
            cycleNode.children = parseTemplateTokens(tpl, pos, openTag, closeTag, L"endcycle", L"", dummyHit);
            nodes.push_back(cycleNode);
        }
        else if (directive.rfind(L"if ", 0) == 0) {
            TplNode ifNode;
            ifNode.type = NodeType::IF_COND;
            ifNode.content = trimWString(directive.substr(3));

            std::wstring branchHit;
            ifNode.children = parseTemplateTokens(tpl, pos, openTag, closeTag, L"else", L"endif", branchHit);

            if (branchHit == L"else") {
                ifNode.elseChildren = parseTemplateTokens(tpl, pos, openTag, closeTag, L"endif", L"", branchHit);
            }

            nodes.push_back(ifNode);
        }
        else {
            TplNode varNode;
            varNode.type = NodeType::VAR;
            varNode.content = directive;

            if (directive.find(L"| raw") != std::wstring::npos) {
                varNode.isRaw = true;
            } else if (directive.find(L"| rtf") != std::wstring::npos) {
                varNode.isRtf = true;
            }

            nodes.push_back(varNode);
        }
    }

    return nodes;
}

static void renderNodes(
    const std::vector<TplNode>& nodes,
    std::vector<Scope>& scopeStack,
    bool globalEscapeRtf,
    std::wstringstream& out
) {
    for (const auto& node : nodes) {
        if (node.type == NodeType::TEXT) {
            out << node.content;
        }
        else if (node.type == NodeType::VAR) {
            // 1. Obținem valoarea brută din context
			vData val = resolveVar(node.content, scopeStack);
			std::wstring strVal = val.toWString();

			// 2. Aplicăm filtrele dinamice (number_format, date_format etc.)
			strVal = applyFilterChain(strVal, node.content);

			// 3. Aplicăm escaparea RTF/HTML finală
			bool doRtf = (globalEscapeRtf || node.isRtf) && !node.isRaw;
			if (doRtf) {
				out << escapeRTFString(strVal);
			} else {
				out << strVal;
			}
        }
        else if (node.type == NodeType::CYCLE) {
            vData arrVal = resolveVar(node.arrayExpr, scopeStack);
            if (arrVal.isArray() && arrVal.rawArray() != nullptr) {
                auto* rawArr = arrVal.rawArray();
                std::wstring itemKey = node.itemVarName;

                for (const auto& elem : *rawArr) {
                    Scope loopScope;
                    loopScope[itemKey] = elem;
                    if (!itemKey.empty() && itemKey[0] == L'$') {
                        loopScope[itemKey.substr(1)] = elem;
                    } else {
                        loopScope[L"$" + itemKey] = elem;
                    }

                    scopeStack.push_back(loopScope);
                    renderNodes(node.children, scopeStack, globalEscapeRtf, out);
                    scopeStack.pop_back();
                }
            }
        }
        else if (node.type == NodeType::IF_COND) {
            bool condResult = evaluateCondition(node.content, scopeStack);
            if (condResult) {
                renderNodes(node.children, scopeStack, globalEscapeRtf, out);
            } else {
                renderNodes(node.elseChildren, scopeStack, globalEscapeRtf, out);
            }
        }
    }
}

// ============================================================================
// CONFIGURARE SI EXECUȚIE INTERNĂ
// ============================================================================

struct TemplateConfig {
    std::wstring openTag = L"{{";
    std::wstring closeTag = L"}}";
    bool escapeRtf = false;
};

static TemplateConfig parseOptionsFromArgs(const std::vector<vData>& args, size_t startIndex) {
    TemplateConfig cfg;

    if (args.size() <= startIndex) return cfg;

    const vData& opt = args[startIndex];

    if (opt.isBool() || opt.isInt()) {
        cfg.escapeRtf = opt.toBool();
    }
    else if (opt.isMap() && opt.rawMap() != nullptr) {
        auto* m = opt.rawMap();

        auto itRtf = m->find(L"rtf");
        if (itRtf != m->end()) cfg.escapeRtf = itRtf->second.toBool();

        auto itOpen = m->find(L"open");
        if (itOpen != m->end()) cfg.openTag = itOpen->second.toWString();

        auto itClose = m->find(L"close");
        if (itClose != m->end()) cfg.closeTag = itClose->second.toWString();
    }
    else if (opt.isString()) {
        std::wstring strOpt = opt.toWString();
        if (strOpt == L"rtf") cfg.escapeRtf = true;
    }

    if (args.size() > startIndex + 1) {
        cfg.openTag = args[startIndex + 1].toWString();
    }
    if (args.size() > startIndex + 2) {
        cfg.closeTag = args[startIndex + 2].toWString();
    }

    return cfg;
}

static vData renderTemplateStringInternal(const std::wstring& tplContent, const vData& dataContext, const TemplateConfig& cfg) {
    std::vector<Scope> scopeStack;
    Scope globalScope;

    if (dataContext.isMap() && dataContext.rawMap() != nullptr) {
        auto* m = dataContext.rawMap();
        for (const auto& [k, v] : *m) {
            globalScope[k] = v;
            if (!k.empty() && k[0] != L'$') {
                globalScope[L"$" + k] = v;
            }
        }
    }
    scopeStack.push_back(globalScope);

    size_t pos = 0;
    std::wstring dummyHit;
    std::vector<TplNode> ast = parseTemplateTokens(tplContent, pos, cfg.openTag, cfg.closeTag, L"", L"", dummyHit);

    std::wstringstream resultStream;
    renderNodes(ast, scopeStack, cfg.escapeRtf, resultStream);

    return vData(resultStream.str());
}


// Helper generic pentru apel de funcții native/plugin
static vData executeEngineFunction(const std::wstring& funcName, const std::vector<vData>& args, const PluginRegistry& globalRegistry) {
    std::wstring uName = funcName;
    for (auto& c : uName) c = std::towupper(c);

    // 1. Căutare în registrul dinamic de funcții (NATIVE + PLUGINS)
    auto it = globalRegistry.find(uName);
    if (it != globalRegistry.end()) {
        return it->second(args); // Apel direct al handler-ului din engine
    }

    // 2. Fallback / Truncare dacă nu este găsită
    return vData(L"");
}


// ============================================================================
// REGISTRARE FUNCȚII PLUGIN
// ============================================================================

void RegisterTplFunctions(PluginRegistry& registry) {

    registry[L"TPL_ESCAPE_RTF"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(L"");
        return vData(escapeRTFString(args[0].toWString()));
    };

    registry[L"TPL_RENDER_STRING"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) {
            return vData(L"ERR_TPL_INVALID_ARGS");
        }

        std::wstring tplContent = args[0].toWString();
        vData dataContext = args[1];

        TemplateConfig cfg = parseOptionsFromArgs(args, 2);
        return renderTemplateStringInternal(tplContent, dataContext, cfg);
    };

    registry[L"TPL_RENDER_FILE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) {
            return vData(L"ERR_TPL_INVALID_ARGS: Necesită cel puțin (tpl_path, data_map)");
        }

        std::wstring tplPath = args[0].toWString();
        vData dataContext = args[1];

        std::wstring outputPath = L"";
        size_t optionsStartIndex = 2;

        // Dacă al 3-lea parametru este cale de fișier de ieșire (String care nu e un flag simplu precum "rtf")
        if (args.size() >= 3) {
            const vData& arg2 = args[2];
            if (arg2.isString()) {
                std::wstring s = arg2.toWString();
                if (s != L"rtf" && s != L"raw" && s != L"html" && s != L"txt") {
                    outputPath = s;
                    optionsStartIndex = 3;
                }
            }
        }

        TemplateConfig cfg = parseOptionsFromArgs(args, optionsStartIndex);

        // Detectare automată RTF după extensia fișierului
        if (tplPath.find(L".rtf") != std::wstring::npos || outputPath.find(L".rtf") != std::wstring::npos) {
            cfg.escapeRtf = true;
        }

        // Citire șablon de pe disc
        std::wstring content = readTextFileUtf8(tplPath);
        if (content.empty()) {
            return vData(L"ERR_TPL_FILE_NOT_FOUND_OR_EMPTY: " + tplPath);
        }

        // Randare în memorie
        vData renderedResult = renderTemplateStringInternal(content, dataContext, cfg);

        // Caz 1: Dacă s-a specificat cale de salvare pe disc -> scriem fișierul și returnăm bool (true/false)
        if (!outputPath.empty()) {
            bool saved = writeTextFileUtf8(outputPath, renderedResult.toWString());
            return vData(saved);
        }

        // Caz 2: Fără cale de ieșire -> returnăm textul randat direct ca String în memorie!
        return renderedResult;
    };
}

// ============================================================================
// EXPORT INTERFAȚĂ PLUGIN
// ============================================================================

extern "C" {

    OLI_EXPORT void LoadOliPlugin(PluginRegistry &registry)
    {
        RegisterTplFunctions(registry);
    }

    OLI_EXPORT void SetPluginConsoleManager(ConsoleManager *hostCm)
    {
        if (hostCm != nullptr)
        {
            ConsoleManager::setInstance(hostCm);
        }
    }

}