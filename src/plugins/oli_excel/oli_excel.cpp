#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../OliEngine.hpp"
#include "../../vData.hpp"
#include "../../ConsoleManager.hpp"
#include "../../OliKeyWords.hpp"

#include <OpenXLSX.hpp>

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <sstream>
#include <cctype>

#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#include <windows.h>
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#include <clocale>
#endif

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

static std::wstring utf8ToWstring(const std::string& str) {
    if (str.empty()) return L"";
#if defined(_WIN32) || defined(_WIN64)
    int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0);
    std::wstring wstr(count, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &wstr[0], count);
    return wstr;
#else
    return L"";
#endif
}

static std::string wstringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
#if defined(_WIN32) || defined(_WIN64)
    int count = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
    if (count <= 0) return "";
    std::string str(count, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &str[0], count, NULL, NULL);
    return str;
#else
    return "";
#endif
}

// Helper: caută foaia cerută; dacă nu există, o returnează pe prima din fișier
static OpenXLSX::XLWorksheet getWorksheetSafe(OpenXLSX::XLDocument& doc, const std::string& sheetName) {
    if (doc.workbook().worksheetExists(sheetName)) {
        return doc.workbook().worksheet(sheetName);
    }
    auto names = doc.workbook().sheetNames();
    if (!names.empty()) {
        return doc.workbook().worksheet(names[0]);
    }
    throw std::runtime_error("Nu exista foi de lucru in document.");
}

// =========================================================================
// NOU: Funcția Smart Centralizată (transformă stringurile în numere reale)
// =========================================================================
static void setSmartCellValue(OpenXLSX::XLCell& cell, std::string strVal) {
    // 1. Curățăm spațiile goale (trim)
    strVal.erase(0, strVal.find_first_not_of(" \t\r\n"));
    strVal.erase(strVal.find_last_not_of(" \t\r\n") + 1);

    if (strVal.empty()) {
        cell.value() = "";
        return;
    }

    // 2. Normalizăm virgula în punct (în caz că STR() a produs "123,45")
    std::string normalizedStr = strVal;
    for (char& ch : normalizedStr) {
        if (ch == ',') ch = '.';
    }

    // 3. Verificăm dacă este un număr
    bool isNumber = true;
    int dotCount = 0;
    for (size_t idx = 0; idx < normalizedStr.length(); ++idx) {
        char ch = normalizedStr[idx];
        if (idx == 0 && (ch == '-' || ch == '+')) continue;
        if (ch == '.') { dotCount++; continue; }
        if (!std::isdigit(static_cast<unsigned char>(ch))) { isNumber = false; break; }
    }

    // 4. Inserăm ca număr pur dacă verificarea a trecut
    if (isNumber && dotCount <= 1 && normalizedStr != "-" && normalizedStr != "+" && normalizedStr != ".") {
        try {
            double numVal = 0.0;
            std::istringstream iss(normalizedStr);
            iss.imbue(std::locale("C")); // Forțăm parsarea cu punct
            if (iss >> numVal) {
                cell.value() = numVal; // <--- Excel îl va vedea ca număr!
                return;
            }
        }
        catch (...) {}
    }

    // 5. Fallback la formatul text dacă este cu adevărat un cuvânt/text
    cell.value() = strVal;
}
// =========================================================================

void RegisterExcelFunctions(PluginRegistry& registry) {

    // 1. EXCEL_CREATE
    registry[L"EXCEL_CREATE"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(false);
        std::string pathUtf8 = wstringToUtf8(args[0].toWString());
        try {
            OpenXLSX::XLDocument doc;
            doc.create(pathUtf8, OpenXLSX::XLForceOverwrite);
            doc.save();
            doc.close();
            return vData(true);
        }
        catch (...) { return vData(false); }
    };

    // 2. EXCEL_WRITE_CELL
    registry[L"EXCEL_WRITE_CELL"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData(false);
        std::string pathUtf8 = wstringToUtf8(args[0].toWString());
        std::string sheetUtf8 = wstringToUtf8(args[1].toWString());
        std::string cellUtf8 = wstringToUtf8(args[2].toWString());
        vData val = args[3];

        try {
            OpenXLSX::XLDocument doc; doc.open(pathUtf8);
            auto wks = getWorksheetSafe(doc, sheetUtf8);
            auto cell = wks.cell(cellUtf8);

            if (val.isNumber()) {
                if (val.isInt()) cell.value() = val.toInt();
                else cell.value() = val.toDouble();
            } else if (val.isBool()) {
                cell.value() = val.toBool();
            } else {
                setSmartCellValue(cell, wstringToUtf8(val.toWString()));
            }

            doc.save(); doc.close();
            return vData(true);
        } catch (...) { return vData(false); }
    };

    // 3. EXCEL_READ_CELL
    registry[L"EXCEL_READ_CELL"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData(L"");
        std::string pathUtf8 = wstringToUtf8(args[0].toWString());
        std::string sheetUtf8 = wstringToUtf8(args[1].toWString());
        std::string cellUtf8 = wstringToUtf8(args[2].toWString());
        try {
            OpenXLSX::XLDocument doc; doc.open(pathUtf8);
            auto wks = getWorksheetSafe(doc, sheetUtf8);
            auto cell = wks.cell(cellUtf8);
            std::string cellVal = cell.value().getString();
            doc.close();
            return vData(utf8ToWstring(cellVal));
        } catch (...) { return vData(L""); }
    };

    // 4. EXCEL_APPEND_ROW
    registry[L"EXCEL_APPEND_ROW"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData(false);
        std::string pathUtf8 = wstringToUtf8(args[0].toWString());
        std::string sheetUtf8 = wstringToUtf8(args[1].toWString());
        vData arrVal = args[2];
        int rowIndex = (int)args[3].toInt();

        if (!arrVal.isArray() || arrVal.rawArray() == nullptr) return vData(false);

        try {
            OpenXLSX::XLDocument doc; doc.open(pathUtf8);
            auto wks = getWorksheetSafe(doc, sheetUtf8);
            auto* arr = arrVal.rawArray();
            int colIndex = 1;

            for (const auto& item : *arr) {
                char colLetter = 'A' + (colIndex - 1);
                std::string cellRef = std::string(1, colLetter) + std::to_string(rowIndex);
                auto cell = wks.cell(cellRef);

                if (item.isNumber()) {
                    if (item.isInt()) cell.value() = item.toInt();
                    else cell.value() = item.toDouble();
                } else if (item.isBool()) {
                    cell.value() = item.toBool();
                } else {
                    setSmartCellValue(cell, wstringToUtf8(item.toWString()));
                }
                colIndex++;
            }
            doc.save(); doc.close();
            return vData(true);
        } catch (...) { return vData(false); }
    };

    // 5. EXCEL_CLONE
    registry[L"EXCEL_CLONE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{ false };
        try {
            std::string src = wstringToUtf8(args[0].toWString());
            std::string dst = wstringToUtf8(args[1].toWString());

            std::filesystem::path dstPath(dst);
            if (dstPath.has_parent_path()) {
                std::filesystem::create_directories(dstPath.parent_path());
            }

            if (!std::filesystem::exists(src)) return vData{ false };
            std::filesystem::copy(src, dst, std::filesystem::copy_options::overwrite_existing);
            return vData{ true };
        } catch (...) { return vData{ false }; }
    };

    // 6. EXCEL_REPLACE_VAR
    registry[L"EXCEL_REPLACE_VAR"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{ false };
        try {
            std::string path = wstringToUtf8(args[0].toWString());
            std::string sheet = wstringToUtf8(args[1].toWString());
            std::string searchStr = wstringToUtf8(args[2].toWString());
            std::string replaceStr = wstringToUtf8(args[3].toWString());

            OpenXLSX::XLDocument doc; doc.open(path);
            auto wks = getWorksheetSafe(doc, sheet);

            for (uint32_t r = 1; r <= wks.rowCount(); ++r) {
                for (uint16_t c = 1; c <= wks.columnCount(); ++c) {
                    auto cell = wks.cell(r, c);
                    if (cell.value().type() == OpenXLSX::XLValueType::String) {
                        std::string val = cell.value().get<std::string>();
                        size_t pos = val.find(searchStr);
                        if (pos != std::string::npos) {
                            val.replace(pos, searchStr.length(), replaceStr);
                            setSmartCellValue(cell, val); // <--- AICI e corecția
                        }
                    }
                }
            }
            doc.save(); doc.close();
            return vData{ true };
        } catch (...) { return vData{ false }; }
    };

    // 6b. EXCEL_REPLACE_MAP
    registry[L"EXCEL_REPLACE_MAP"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ false };
        try {
            std::string path = wstringToUtf8(args[0].toWString());
            std::string sheet = wstringToUtf8(args[1].toWString());
            const vData& mapData = args[2];

            auto* rawM = mapData.rawMap();
            if (!rawM) return vData{ false };

            std::vector<std::pair<std::string, std::string>> replacements;
            for (const auto& [k, v] : *rawM) {
                replacements.push_back({ wstringToUtf8(k), wstringToUtf8(v.toWString()) });
            }

            OpenXLSX::XLDocument doc; doc.open(path);
            auto wks = getWorksheetSafe(doc, sheet);

            for (uint32_t r = 1; r <= wks.rowCount(); ++r) {
                for (uint16_t c = 1; c <= wks.columnCount(); ++c) {
                    auto cell = wks.cell(r, c);
                    if (cell.value().type() == OpenXLSX::XLValueType::String) {
                        std::string val = cell.value().get<std::string>();
                        bool modified = false;

                        for (const auto& [searchStr, replaceStr] : replacements) {
                            size_t pos = val.find(searchStr);
                            if (pos != std::string::npos) {
                                val.replace(pos, searchStr.length(), replaceStr);
                                modified = true;
                            }
                        }

                        if (modified) {
                            setSmartCellValue(cell, val); // <--- AICI e corecția majoră
                        }
                    }
                }
            }
            doc.save(); doc.close();
            return vData{ true };
        } catch (...) { return vData{ false }; }
    };

    // 7. EXCEL_FIND_MARKER
    registry[L"EXCEL_FIND_MARKER"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ false };
        try {
            std::string path = wstringToUtf8(args[0].toWString());
            std::string sheet = wstringToUtf8(args[1].toWString());
            std::string marker = wstringToUtf8(args[2].toWString());

            OpenXLSX::XLDocument doc; doc.open(path);
            auto wks = getWorksheetSafe(doc, sheet);

            for (uint32_t r = 1; r <= wks.rowCount(); ++r) {
                for (uint16_t c = 1; c <= wks.columnCount(); ++c) {
                    auto cell = wks.cell(r, c);
                    if (cell.value().type() == OpenXLSX::XLValueType::String) {
                        if (cell.value().get<std::string>() == marker) {
                            doc.close();
                            vData res = vData::CreateArray();
                            auto* arr = res.rawArray();
                            if (arr) {
                                arr->push_back(vData{ static_cast<long long>(r) });
                                arr->push_back(vData{ static_cast<long long>(c) });
                            }
                            return res;
                        }
                    }
                }
            }
            doc.close();
        } catch (...) {}
        return vData{ false };
    };

    // 8. EXCEL_WRITE_SMART_RC
    registry[L"EXCEL_WRITE_SMART_RC"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 5) return vData{ false };
        try {
            std::string path = wstringToUtf8(args[0].toWString());
            std::string sheet = wstringToUtf8(args[1].toWString());
            uint32_t r = static_cast<uint32_t>(args[2].toInt());
            uint16_t c = static_cast<uint16_t>(args[3].toInt());
            std::string strVal = wstringToUtf8(args[4].toWString());

            OpenXLSX::XLDocument doc; doc.open(path);
            auto wks = getWorksheetSafe(doc, sheet);
            auto cell = wks.cell(r, c);

            setSmartCellValue(cell, strVal); // <--- Refolosește logica centralizată

            doc.save(); doc.close();
            return vData{ true };
        } catch (...) { return vData{ false }; }
    };

    // 9. EXCEL_REFRESH_FORMULAS
    registry[L"EXCEL_REFRESH_FORMULAS"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{ false };
        try {
            std::string path = wstringToUtf8(args[0].toWString());
            std::string sheet = wstringToUtf8(args[1].toWString());

            OpenXLSX::XLDocument doc; doc.open(path);
            auto wks = getWorksheetSafe(doc, sheet);

            for (uint32_t r = 1; r <= wks.rowCount(); ++r) {
                for (uint16_t c = 1; c <= wks.columnCount(); ++c) {
                    auto cell = wks.cell(r, c);
                    if (cell.hasFormula()) {
                        std::string formulaStr = cell.formula().get();
                        cell.formula() = formulaStr;
                    }
                }
            }
            doc.save(); doc.close();
            return vData{ true };
        } catch (...) { return vData{ false }; }
    };
}

extern "C" {
    OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {
        RegisterExcelFunctions(registry);
    }

    OLI_EXPORT void SetPluginConsoleManager(ConsoleManager* hostCm) {
        if (hostCm != nullptr) {
            ConsoleManager::setInstance(hostCm);
        }
    }
}