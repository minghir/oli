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

void RegisterExcelFunctions(PluginRegistry& registry) {

    // 1. EXCEL_CREATE(path)
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
        catch (const std::exception& e) {
            LOG_INFO(L"[EXCEL_ERR] " + utf8ToWstring(e.what()));
            return vData(false);
        }
    };

    // 2. EXCEL_WRITE_CELL(path, sheet_name, cell_ref, value)
    registry[L"EXCEL_WRITE_CELL"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData(false);

        std::string pathUtf8 = wstringToUtf8(args[0].toWString());
        std::string sheetUtf8 = wstringToUtf8(args[1].toWString());
        std::string cellUtf8 = wstringToUtf8(args[2].toWString());
        vData val = args[3];

        try {
            OpenXLSX::XLDocument doc;
            doc.open(pathUtf8);

            auto wks = getWorksheetSafe(doc, sheetUtf8);
            auto cell = wks.cell(cellUtf8);

            if (val.isNumber()) {
                if (val.isInt()) cell.value() = val.toInt();
                else cell.value() = val.toDouble();
            }
            else if (val.isBool()) {
                cell.value() = val.toBool();
            }
            else {
                cell.value() = wstringToUtf8(val.toWString());
            }

            doc.save();
            doc.close();
            return vData(true);
        }
        catch (const std::exception& e) {
            LOG_INFO(L"[EXCEL_ERR] " + utf8ToWstring(e.what()));
            return vData(false);
        }
    };

    // 3. EXCEL_READ_CELL(path, sheet_name, cell_ref)
    registry[L"EXCEL_READ_CELL"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData(L"");

        std::string pathUtf8 = wstringToUtf8(args[0].toWString());
        std::string sheetUtf8 = wstringToUtf8(args[1].toWString());
        std::string cellUtf8 = wstringToUtf8(args[2].toWString());

        try {
            OpenXLSX::XLDocument doc;
            doc.open(pathUtf8);

            auto wks = getWorksheetSafe(doc, sheetUtf8);
            auto cell = wks.cell(cellUtf8);

            std::string cellVal = cell.value().getString();
            doc.close();

            return vData(utf8ToWstring(cellVal));
        }
        catch (const std::exception& e) {
            LOG_INFO(L"[EXCEL_ERR] " + utf8ToWstring(e.what()));
            return vData(L"");
        }
    };

    // 4. EXCEL_APPEND_ROW(path, sheet_name, array_values, row_index)
    registry[L"EXCEL_APPEND_ROW"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData(false);

        std::string pathUtf8 = wstringToUtf8(args[0].toWString());
        std::string sheetUtf8 = wstringToUtf8(args[1].toWString());
        vData arrVal = args[2];
        int rowIndex = (int)args[3].toInt();

        if (!arrVal.isArray() || arrVal.rawArray() == nullptr) {
            return vData(false);
        }

        try {
            OpenXLSX::XLDocument doc;
            doc.open(pathUtf8);

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
                }
                else if (item.isBool()) {
                    cell.value() = item.toBool();
                }
                else {
                    cell.value() = wstringToUtf8(item.toWString());
                }

                colIndex++;
            }

            doc.save();
            doc.close();
            return vData(true);
        }
        catch (const std::exception& e) {
            LOG_INFO(L"[EXCEL_ERR] " + utf8ToWstring(e.what()));
            return vData(false);
        }
    };

    // 5. EXCEL_CLONE(tpl_path, dest_path)
    registry[L"EXCEL_CLONE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{ false };
        try {
            std::string src = wstringToUtf8(args[0].toWString());
            std::string dst = wstringToUtf8(args[1].toWString());

            std::filesystem::path dstPath(dst);
            if (dstPath.has_parent_path()) {
                std::filesystem::create_directories(dstPath.parent_path());
            }

            if (!std::filesystem::exists(src)) {
                return vData{ false };
            }

            std::filesystem::copy(src, dst, std::filesystem::copy_options::overwrite_existing);
            return vData{ true };
        } catch (...) { 
            return vData{ false }; 
        }
    };

    // 6. EXCEL_REPLACE_VAR(path, sheet, var_name, var_value)
    registry[L"EXCEL_REPLACE_VAR"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{ false };
        try {
            std::string path = wstringToUtf8(args[0].toWString());
            std::string sheet = wstringToUtf8(args[1].toWString());
            std::string searchStr = wstringToUtf8(args[2].toWString());
            std::string replaceStr = wstringToUtf8(args[3].toWString());

            OpenXLSX::XLDocument doc; 
            doc.open(path);
            auto wks = getWorksheetSafe(doc, sheet);

            for (uint32_t r = 1; r <= wks.rowCount(); ++r) {
                for (uint16_t c = 1; c <= wks.columnCount(); ++c) {
                    auto cell = wks.cell(r, c);
                    if (cell.value().type() == OpenXLSX::XLValueType::String) {
                        std::string val = cell.value().get<std::string>();
                        size_t pos = val.find(searchStr);
                        if (pos != std::string::npos) {
                            val.replace(pos, searchStr.length(), replaceStr);
                            cell.value() = val;
                        }
                    }
                }
            }
            doc.save(); 
            doc.close();
            return vData{ true };
        } catch (...) { 
            return vData{ false }; 
        }
    };

    // 6b. EXCEL_REPLACE_MAP(path, sheet, map_vars) -> Înlocuiește un MAP întreg dintr-o singură trecere!
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

            OpenXLSX::XLDocument doc; 
            doc.open(path);
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
                            cell.value() = val;
                        }
                    }
                }
            }
            doc.save(); 
            doc.close();
            return vData{ true };
        } catch (...) { 
            return vData{ false }; 
        }
    };

    // 7. EXCEL_FIND_MARKER(path, sheet, marker)
    registry[L"EXCEL_FIND_MARKER"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ false };
        try {
            std::string path = wstringToUtf8(args[0].toWString());
            std::string sheet = wstringToUtf8(args[1].toWString());
            std::string marker = wstringToUtf8(args[2].toWString());

            OpenXLSX::XLDocument doc; 
            doc.open(path);
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

    // 8. EXCEL_WRITE_SMART_RC(path, sheet, row, col, value)
    registry[L"EXCEL_WRITE_SMART_RC"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 5) return vData{ false };
        try {
            std::string path = wstringToUtf8(args[0].toWString());
            std::string sheet = wstringToUtf8(args[1].toWString());
            uint32_t r = static_cast<uint32_t>(args[2].toInt());
            uint16_t c = static_cast<uint16_t>(args[3].toInt());
            std::string strVal = wstringToUtf8(args[4].toWString());

            OpenXLSX::XLDocument doc; 
            doc.open(path);
            auto wks = getWorksheetSafe(doc, sheet);
            auto cell = wks.cell(r, c);

            if (strVal.empty()) {
                cell.value() = "";
            } else {
                bool isNumber = true; 
                int dotCount = 0;
                for (size_t idx = 0; idx < strVal.length(); ++idx) {
                    char ch = strVal[idx];
                    if (idx == 0 && ch == '-') continue;
                    if (ch == '.') { dotCount++; continue; }
                    if (!isdigit(ch)) { isNumber = false; break; }
                }

                if (isNumber && dotCount <= 1 && strVal != "-" && strVal != ".") {
                    try {
                        double numVal = 0.0;
                        std::istringstream iss(strVal);
                        iss.imbue(std::locale("C"));
                        if (iss >> numVal) {
                            cell.value() = numVal;
                        } else {
                            cell.value() = strVal;
                        }
                    } catch (...) { 
                        cell.value() = strVal; 
                    }
                } else {
                    cell.value() = strVal;
                }
            }
            doc.save(); 
            doc.close();
            return vData{ true };
        } catch (...) { 
            return vData{ false }; 
        }
    };

    // 9. EXCEL_REFRESH_FORMULAS(path, sheet)
    registry[L"EXCEL_REFRESH_FORMULAS"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{ false };
        try {
            std::string path = wstringToUtf8(args[0].toWString());
            std::string sheet = wstringToUtf8(args[1].toWString());

            OpenXLSX::XLDocument doc; 
            doc.open(path);
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
            doc.save(); 
            doc.close();
            return vData{ true };
        } catch (...) { 
            return vData{ false }; 
        }
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

/*
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

void RegisterExcelFunctions(PluginRegistry& registry) {

    // 1. EXCEL_CREATE(path)
    registry[L"EXCEL_CREATE"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(false);
        std::string pathUtf8 = wstringToUtf8(args[0].toWString());

        try {
            OpenXLSX::XLDocument doc;
            doc.create(pathUtf8.c_str()); // Folosim c_str() pentru compatibilitate
            doc.save();
            doc.close();
            return vData(true);
        }
        catch (const std::exception& e) {
            LOG_INFO(L"[EXCEL_ERR] " + utf8ToWstring(e.what()));
            return vData(false);
        }
    };

    // 2. EXCEL_WRITE_CELL(path, sheet_name, cell_ref, value)
    registry[L"EXCEL_WRITE_CELL"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData(false);

        std::string pathUtf8 = wstringToUtf8(args[0].toWString());
        std::string sheetUtf8 = wstringToUtf8(args[1].toWString());
        std::string cellUtf8 = wstringToUtf8(args[2].toWString());
        vData val = args[3];

        try {
            OpenXLSX::XLDocument doc;
            doc.open(pathUtf8);

            auto wbk = doc.workbook();
            auto wks = wbk.worksheet(sheetUtf8);
            auto cell = wks.cell(cellUtf8);

            // Atribuire directă cu operatorul =
            if (val.isNumber()) {
                if (val.isInt()) cell.value() = val.toInt();
                else cell.value() = val.toDouble();
            }
            else if (val.isBool()) {
                cell.value() = val.toBool();
            }
            else {
                cell.value() = wstringToUtf8(val.toWString());
            }

            doc.save();
            doc.close();
            return vData(true);
        }
        catch (const std::exception& e) {
            LOG_INFO(L"[EXCEL_ERR] " + utf8ToWstring(e.what()));
            return vData(false);
        }
    };

    // 3. EXCEL_READ_CELL(path, sheet_name, cell_ref)
    registry[L"EXCEL_READ_CELL"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData(L"");

        std::string pathUtf8 = wstringToUtf8(args[0].toWString());
        std::string sheetUtf8 = wstringToUtf8(args[1].toWString());
        std::string cellUtf8 = wstringToUtf8(args[2].toWString());

        try {
            OpenXLSX::XLDocument doc;
            doc.open(pathUtf8);

            auto wbk = doc.workbook();
            auto wks = wbk.worksheet(sheetUtf8);
            auto cell = wks.cell(cellUtf8);

            std::string cellVal = cell.value().getString();
            doc.close();

            return vData(utf8ToWstring(cellVal));
        }
        catch (const std::exception& e) {
            LOG_INFO(L"[EXCEL_ERR] " + utf8ToWstring(e.what()));
            return vData(L"");
        }
    };
    // 4. EXCEL_APPEND_ROW(path, sheet_name, array_values, row_index) 
    // Sau o variantă care determină singură sau primește rândul curent
    registry[L"EXCEL_APPEND_ROW"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData(false);

        std::string pathUtf8 = wstringToUtf8(args[0].toWString());
        std::string sheetUtf8 = wstringToUtf8(args[1].toWString());
        vData arrVal = args[2];
        int rowIndex = (int)args[3].toInt(); // Rândul la care scriem (ex: 5, 6, 7...)

        if (!arrVal.isArray() || arrVal.rawArray() == nullptr) {
            return vData(false);
        }

        try {
            OpenXLSX::XLDocument doc;
            doc.open(pathUtf8);

            auto wbk = doc.workbook();
            auto wks = wbk.worksheet(sheetUtf8);

            auto* arr = arrVal.rawArray();
            int colIndex = 1; // Începem de la coloana A (1 = A, 2 = B, etc.)

            for (const auto& item : *arr) {
                // Generăm litera coloanei (A, B, C, D...) bazat pe colIndex
                // Pentru rapoarte standard de până la 26 de coloane:
                char colLetter = 'A' + (colIndex - 1);
                std::string cellRef = std::string(1, colLetter) + std::to_string(rowIndex);

                auto cell = wks.cell(cellRef);

                if (item.isNumber()) {
                    if (item.isInt()) cell.value() = item.toInt();
                    else cell.value() = item.toDouble();
                }
                else if (item.isBool()) {
                    cell.value() = item.toBool();
                }
                else {
                    cell.value() = wstringToUtf8(item.toWString());
                }

                colIndex++;
            }

            doc.save();
            doc.close();
            return vData(true);
        }
        catch (const std::exception& e) {
            LOG_INFO(L"[EXCEL_ERR] " + utf8ToWstring(e.what()));
            return vData(false);
        }
    };
	// 5. EXCEL_CLONE(tpl_path, dest_path) -> Copiază template-ul
    registry[L"EXCEL_CLONE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{ false };
        try {
            std::string src = wstringToUtf8(args[0].toWString());
            std::string dst = wstringToUtf8(args[1].toWString());
            std::filesystem::copy(src, dst, std::filesystem::copy_options::overwrite_existing);
            return vData{ true };
        } catch (...) { 
            return vData{ false }; 
        }
    };

    // 6. EXCEL_REPLACE_VAR(path, sheet, var_name, var_value) -> Caută și înlocuiește un string (ex: {DATA_START})
    registry[L"EXCEL_REPLACE_VAR"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{ false };
        try {
            std::string path = wstringToUtf8(args[0].toWString());
            std::string sheet = wstringToUtf8(args[1].toWString());
            std::string searchStr = wstringToUtf8(args[2].toWString());
            std::string replaceStr = wstringToUtf8(args[3].toWString());

            OpenXLSX::XLDocument doc; 
            doc.open(path);
            auto wks = doc.workbook().worksheet(sheet);

            for (uint32_t r = 1; r <= wks.rowCount(); ++r) {
                for (uint16_t c = 1; c <= wks.columnCount(); ++c) {
                    auto cell = wks.cell(r, c);
                    if (cell.value().type() == OpenXLSX::XLValueType::String) {
                        std::string val = cell.value().get<std::string>();
                        size_t pos = val.find(searchStr);
                        if (pos != std::string::npos) {
                            val.replace(pos, searchStr.length(), replaceStr);
                            cell.value() = val;
                        }
                    }
                }
            }
            doc.save(); 
            doc.close();
            return vData{ true };
        } catch (...) { 
            return vData{ false }; 
        }
    };

    // 7. EXCEL_FIND_MARKER(path, sheet, marker) -> Returnează Array: [Rând, Coloană]
    registry[L"EXCEL_FIND_MARKER"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ false };
        try {
            std::string path = wstringToUtf8(args[0].toWString());
            std::string sheet = wstringToUtf8(args[1].toWString());
            std::string marker = wstringToUtf8(args[2].toWString());

            OpenXLSX::XLDocument doc; 
            doc.open(path);
            auto wks = doc.workbook().worksheet(sheet);

            for (uint32_t r = 1; r <= wks.rowCount(); ++r) {
                for (uint16_t c = 1; c <= wks.columnCount(); ++c) {
                    auto cell = wks.cell(r, c);
                    if (cell.value().type() == OpenXLSX::XLValueType::String) {
                        if (cell.value().get<std::string>() == marker) {
                            doc.close();

                            // Folosim factory-ul nativ din vData.hpp
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

    // 8. EXCEL_WRITE_SMART_RC(path, sheet, row, col, value) -> Funcția ta inteligentă
    registry[L"EXCEL_WRITE_SMART_RC"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 5) return vData{ false };
        try {
            std::string path = wstringToUtf8(args[0].toWString());
            std::string sheet = wstringToUtf8(args[1].toWString());
            uint32_t r = static_cast<uint32_t>(args[2].toInt());
            uint16_t c = static_cast<uint16_t>(args[3].toInt());
            std::string strVal = wstringToUtf8(args[4].toWString());

            OpenXLSX::XLDocument doc; 
            doc.open(path);
            auto wks = doc.workbook().worksheet(sheet);
            auto cell = wks.cell(r, c);

            if (strVal.empty()) {
                cell.value() = "";
            } else {
                bool isNumber = true; 
                int dotCount = 0;
                for (size_t idx = 0; idx < strVal.length(); ++idx) {
                    char ch = strVal[idx];
                    if (idx == 0 && ch == '-') continue;
                    if (ch == '.') { dotCount++; continue; }
                    if (!isdigit(ch)) { isNumber = false; break; }
                }

                if (isNumber && dotCount <= 1 && strVal != "-" && strVal != ".") {
                    try {
                        double numVal = 0.0;
                        std::istringstream iss(strVal);
                        iss.imbue(std::locale("C"));
                        if (iss >> numVal) {
                            cell.value() = numVal;
                        } else {
                            cell.value() = strVal;
                        }
                    } catch (...) { 
                        cell.value() = strVal; 
                    }
                } else {
                    cell.value() = strVal;
                }
            }
            doc.save(); 
            doc.close();
            return vData{ true };
        } catch (...) { 
            return vData{ false }; 
        }
    };

    // 9. EXCEL_REFRESH_FORMULAS(path, sheet) -> Hack-ul tău pt formule
    registry[L"EXCEL_REFRESH_FORMULAS"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{ false };
        try {
            std::string path = wstringToUtf8(args[0].toWString());
            std::string sheet = wstringToUtf8(args[1].toWString());

            OpenXLSX::XLDocument doc; 
            doc.open(path);
            auto wks = doc.workbook().worksheet(sheet);

            for (uint32_t r = 1; r <= wks.rowCount(); ++r) {
                for (uint16_t c = 1; c <= wks.columnCount(); ++c) {
                    auto cell = wks.cell(r, c);
                    if (cell.hasFormula()) {
                        std::string formulaStr = cell.formula().get();
                        cell.formula() = formulaStr;
                    }
                }
            }
            doc.save(); 
            doc.close();
            return vData{ true };
        } catch (...) { 
            return vData{ false }; 
        }
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
*/