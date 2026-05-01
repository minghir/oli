#include "OliEngine.hpp"
#include "PortTools.hpp"

#include <fstream>
#include <filesystem>
#include <random>
#include <vector>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>
#include <string_view>
#include <algorithm> // Nec

void vOliEngine::initializeFunctionsHandlers() {

    m_functionsHandlers[L"INCLUDE"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(0LL);

        // Creăm un obiect ShellCommand artificial
        ShellCommand sc;
        sc.args.push_back(vDataToWString(args[0]));

        this->handleRunCommand(sc);
        return vData(1LL);
        };

    m_functionsHandlers[L"REF"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) return { std::monostate{} };

        // Avem nevoie de numele variabilei (ex: REF("a"))
        std::wstring varName = vDataToWString(args[0]);
        if (varName[0] == L'$') varName.erase(0, 1);

        // Căutăm variabila în memorie (Global sau Local)
        vData* targetPtr = nullptr;
        if (!m_callStack.empty()) {
            auto& locals = m_callStack.back().localVariables;
            if (locals.count(varName)) targetPtr = &locals[varName];
        }

        if (!targetPtr && m_globalVariables.count(varName)) {
            targetPtr = &m_globalVariables[varName];
        }

        if (targetPtr) {
            vData refResult;
            refResult.value = targetPtr; // Stocăm adresa brută
            return refResult;
        }

        return { std::monostate{} };
        };

    // --- ISREF(value) -> 1 dacă e pointer, 0 dacă e valoare pură ---
    m_functionsHandlers[L"ISREF"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(0LL);
        bool isRef = std::holds_alternative<vData*>(args[0].value);
        return vData(isRef ? 1LL : 0LL);
        };

    // --- DEREF(ref) -> Extrage valoarea din spatele pointerului ---
    m_functionsHandlers[L"DEREF"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(std::monostate{});

        if (std::holds_alternative<vData*>(args[0].value)) {
            vData* ptr = std::get<vData*>(args[0].value);
            if (ptr) return *ptr; // Returnăm valoarea la care pointează
        }

        // Dacă nu e pointer, returnăm valoarea ca atare (comportament de siguranță)
        return args[0];
        };

    // --- SETREF(ref, value) -> Scrie o valoare nouă la adresa indicată ---
    m_functionsHandlers[L"SETREF"] = [this](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData(0LL);

        if (std::holds_alternative<vData*>(args[0].value)) {
            vData* ptr = std::get<vData*>(args[0].value);
            if (ptr) {
                *ptr = args[1]; // Modificăm memoria originală!
                return vData(1LL); // Succes
            }
        }
        return vData(0LL); // Eșec (nu era un pointer valid)
        };

    m_functionsHandlers[L"CLONE"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) {
            LOG_ERROR(L"[RUNTIME ERROR] CLONE() requires a parameter.");
            return { std::monostate{} };
        }

        // Deoarece handler-ul primeste deja argumentele evaluate,
        // pur si simplu trimitem primul argument catre deepCopy.
        return this->deepCopy(args[0]);
        };

    // Înregistrăm funcția TYPE
    m_functionsHandlers[L"TYPE"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) return { L"NULL" };

        // args[0] este deja vData, nu mai facem evaluateExpression!
        return { getVariantTypeName(args[0]) };
        };

    // Funcția LEN
    m_functionsHandlers[L"LEN"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) return { 0LL };
        const vData& d = args[0];

        // Cazul ARRAY
        if (d.isArray()) {
            auto arrPtr = std::get<vDataArray>(d.value);
            // Folosim ->size() pe pointerul partajat
            return { static_cast<long long>(arrPtr ? arrPtr->size() : 0) };
        }

        // Cazul MAP
        if (d.isMap()) {
            auto mapPtr = std::get<vDataMap>(d.value);
            return { static_cast<long long>(mapPtr ? mapPtr->size() : 0) };
        }

        // Cazul STRING (rămâne neschimbat, wstring nu e pointer)
        if (d.isString()) {
            return { static_cast<long long>(std::get<std::wstring>(d.value).size()) };
        }

        return { 0LL };
        };



    m_functionsHandlers[L"INPUT"] = [this](const std::vector<vData>& args) -> vData {
        return this->handleInputFunc(args);
        };

    m_functionsHandlers[L"RANDOM"] = [this](const std::vector<vData>& args) -> vData {
        return this->handleRandomFunc(args);
        };

    m_functionsHandlers[L"RND"] = m_functionsHandlers[L"RANDOM"];

    m_functionsHandlers[L"HASH"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(0LL);

        std::wstring str = vDataToWString(args[0]);
        std::hash<std::wstring> hasher;
        size_t hashValue = hasher(str);

        return vData((long long)hashValue);
        };


    m_functionsHandlers[L"WAIT"] = [this](const std::vector<vData>& args) -> vData {
        return this->handleWaitFunc(args);
        };

    m_functionsHandlers[L"SYS"] = [this](const std::vector<vData>& args) -> vData {
        return this->handleSysFunc(args);
        };

    m_functionsHandlers[L"CONTAINS"] = [this](const std::vector<vData>& args) -> vData {
        return this->handleContainsFunc(args);
        };

    m_functionsHandlers[L"EVAL"] = [this](const std::vector<vData>& args) -> vData {
        return this->handleEvalFunc(args);
        };

    m_functionsHandlers[L"INT"] = [this](const std::vector<vData>& args) -> vData {
        return this->handleIntFunc(args);
        };

    m_functionsHandlers[L"FLOAT"] = [this](const std::vector<vData>& args) -> vData {
        return this->handleFloatFunc(args);
        };

    m_functionsHandlers[L"STR"] = [this](const std::vector<vData>& args) -> vData {
        return this->handleStrFunc(args);
        };
    m_functionsHandlers[L"STRING"] = m_functionsHandlers[L"STR"];

    m_functionsHandlers[L"ARRAY"] = [this](const auto& args) { return handleArrayFunc(args); };

    m_functionsHandlers[L"MAP"] = [this](const auto& args) { return handleMapFunc(args); };

    m_functionsHandlers[L"TRIM"] = [this](const auto& args) { return handleTrimFunc(args); };

    //functii array
    // --- PUSH(array, value) -> Adaugă la final ---
    m_functionsHandlers[L"PUSH"] = [this](const std::vector<vData>& args) -> vData {
        if (args.size() < 2 || !args[0].isArray()) return { 0LL };
        auto arrPtr = std::get<vDataArray>(args[0].value);
        if (arrPtr) {
            arrPtr->push_back(args[1]);
            return { static_cast<long long>(arrPtr->size()) };
        }
        return { 0LL };
        };

    // --- POP(array) -> Scoate de la final și returnează valoarea ---
    m_functionsHandlers[L"POP"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty() || !args[0].isArray()) return { std::monostate{} };
        auto arrPtr = std::get<vDataArray>(args[0].value);
        if (arrPtr && !arrPtr->empty()) {
            vData val = arrPtr->back();
            arrPtr->pop_back();
            return val;
        }
        return { std::monostate{} };
        };

    // --- SHIFT(array) -> Scoate de la început și returnează ---
    m_functionsHandlers[L"SHIFT"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty() || !args[0].isArray()) return { std::monostate{} };
        auto arrPtr = std::get<vDataArray>(args[0].value);
        if (arrPtr && !arrPtr->empty()) {
            vData val = (*arrPtr)[0];
            arrPtr->erase(arrPtr->begin());
            return val;
        }
        return { std::monostate{} };
        };

    // --- UNSHIFT(array, value) -> Adaugă la început ---
    m_functionsHandlers[L"UNSHIFT"] = [this](const std::vector<vData>& args) -> vData {
        if (args.size() < 2 || !args[0].isArray()) return { 0LL };
        auto arrPtr = std::get<vDataArray>(args[0].value);
        if (arrPtr) {
            arrPtr->insert(arrPtr->begin(), args[1]);
            return { static_cast<long long>(arrPtr->size()) };
        }
        return { 0LL };
        };

    // --- INDEXOF(array, value) -> Caută valoarea și returnează indexul sau -1 ---
    m_functionsHandlers[L"INDEXOF"] = [this](const std::vector<vData>& args) -> vData {
        if (args.size() < 2 || !args[0].isArray()) return { -1LL };
        auto arrPtr = std::get<vDataArray>(args[0].value);
        if (arrPtr) {
            for (size_t i = 0; i < arrPtr->size(); ++i) {
                // Presupunem că vData are operatorul == implementat corect
                if ((*arrPtr)[i] == args[1]) return { static_cast<long long>(i) };
            }
        }
        return { -1LL };
        };

    m_functionsHandlers[L"SET_AT"] = [this](const std::vector<vData>& args) -> vData {
        if (args.size() < 3 || !args[0].isArray()) return vData(0LL);

        // Luăm shared_ptr-ul. Orice modificare pe arrPtr->at() 
        // se va vedea în toate variabilele care dețin acest array.
        auto arrPtr = std::get<vDataArray>(args[0].value);
        size_t idx = static_cast<size_t>(vDataToDouble(args[1]));

        if (arrPtr && idx < arrPtr->size()) {
            (*arrPtr)[idx] = args[2]; // <--- Aceasta este scrierea critică
            return vData(1LL);
        }
        return vData(0LL);
        };

    // --- SORT(array) -> Sortează array-ul (In-place) ---
    /*
    m_functionsHandlers[L"SORT"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty() || !args[0].isArray()) return { std::monostate{} };

        auto arrPtr = std::get<vDataArray>(args[0].value);
        if (arrPtr && arrPtr->size() > 1) {
            // Adăugăm "-> bool" pentru a forța deducția corectă
            std::sort(arrPtr->begin(), arrPtr->end(), [](const vData& a, const vData& b) -> bool {
                // 1. Dacă tipurile sunt diferite, le sortăm după indexul variantei
                if (a.value.index() != b.value.index()) {
                    return a.value.index() < b.value.index();
                }

                // 2. Dacă sunt de același tip, facem comparația specifică
                if (a.isInt())
                    return std::get<long long>(a.value) < std::get<long long>(b.value);
                if (a.isFloat())
                    return std::get<double>(a.value) < std::get<double>(b.value);
                if (a.isString())
                    return std::get<std::wstring>(a.value) < std::get<std::wstring>(b.value);

                // Pentru Maps, Arrays sau null, nu avem o ordine naturală
                return false;
                });
        }
        return args[0]; // Returnăm array-ul (care e deja sortat in-place)
        };
    */
    m_functionsHandlers[L"SORT"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty() || !args[0].isArray()) return { std::monostate{} };

        auto arrPtr = std::get<vDataArray>(args[0].value);
        if (arrPtr && arrPtr->size() > 1) {
            std::sort(arrPtr->begin(), arrPtr->end(), [](const vData& a, const vData& b) -> bool {
                if (a.value.index() != b.value.index()) {
                    return a.value.index() < b.value.index();
                }

                if (a.isInt())
                    return std::get<long long>(a.value) < std::get<long long>(b.value);
                if (a.isFloat())
                    return std::get<double>(a.value) < std::get<double>(b.value);

                if (a.isString()) {
                    std::wstring s1 = std::get<std::wstring>(a.value);
                    std::wstring s2 = std::get<std::wstring>(b.value);

                    // --- REPARAȚIE PENTRU TINY BASIC ---
                    // Verificăm dacă ambele string-uri reprezintă numere
                    bool s1Numeric = !s1.empty() && std::all_of(s1.begin(), s1.end(), iswdigit);
                    bool s2Numeric = !s2.empty() && std::all_of(s2.begin(), s2.end(), iswdigit);

                    if (s1Numeric && s2Numeric) {
                        try {
                            // Comparăm valorile numerice, nu caracterele
                            return std::stoll(s1) < std::stoll(s2);
                        }
                        catch (...) {
                            return s1 < s2;
                        }
                    }
                    return s1 < s2;
                }

                return false;
                });
        }
        return args[0];
        };
    //functii pt map-uri
    // --- HASKEY(map, key) -> Returnează 1 dacă cheia există, altfel 0 ---
    m_functionsHandlers[L"HASKEY"] = [this](const std::vector<vData>& args) -> vData {
        if (args.size() < 2 || !args[0].isMap()) return { 0LL };

        auto mapPtr = std::get<vDataMap>(args[0].value);
        std::wstring key = vDataToWString(args[1]);

        if (mapPtr && mapPtr->count(key)) {
            return { 1LL };
        }
        return { 0LL };
        };

    // --- KEYS(map) -> Returnează un ARRAY cu toate cheile (string-uri) ---
    m_functionsHandlers[L"KEYS"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty() || !args[0].isMap()) return vData::CreateArray();

        auto mapPtr = std::get<vDataMap>(args[0].value);
        vData result = vData::CreateArray();
        auto arrPtr = result.rawArray(); // Presupunem că ai acest helper pentru acces la vectorul intern

        if (mapPtr) {
            for (auto const& [key, val] : *mapPtr) {
                arrPtr->push_back(vData(key));
            }
        }
        return result;
        };

    // --- VALUES(map) -> Returnează un ARRAY cu toate valorile ---
    m_functionsHandlers[L"VALUES"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty() || !args[0].isMap()) return vData::CreateArray();

        auto mapPtr = std::get<vDataMap>(args[0].value);
        vData result = vData::CreateArray();
        auto arrPtr = result.rawArray();

        if (mapPtr) {
            for (auto const& [key, val] : *mapPtr) {
                arrPtr->push_back(val); // Aici adăugăm valoarea (deep copy sau referință vData)
            }
        }
        return result;
        };

    // functii pt stringuri
    m_functionsHandlers[L"SPLIT"] = [this](const auto& args) {return this->handleSplitFunc(args); };
    m_functionsHandlers[L"JOIN"] = [this](const auto& args) {return this->handleJoinFunc(args); };
    // --- UPPER(str) ---
    m_functionsHandlers[L"UPPER"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) return { L"" };
        // Folosim to_upper definit in StringUtils.hpp
        return { to_upper(vDataToWString(args[0])) };
        };

    // --- LOWER(str) ---
    m_functionsHandlers[L"LOWER"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) return { L"" };
        // Folosim to_lower definit in StringUtils.hpp
        return { to_lower(vDataToWString(args[0])) };
        };

    // --- REPLACE(str, old, new) ---
    m_functionsHandlers[L"REPLACE"] = [this](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return args.empty() ? vData{ L"" } : args[0];

        std::wstring str = vDataToWString(args[0]);
        std::wstring from = vDataToWString(args[1]);
        std::wstring to = vDataToWString(args[2]);

        // Folosim rpl_wstr_in_wstr din StringUtils.hpp (care face replace global)
        return { rpl_wstr_in_wstr(str, from, to) };
        };

    // --- FIND(str, pattern) ---
    m_functionsHandlers[L"FIND"] = [this](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return { -1LL };

        std::wstring str = vDataToWString(args[0]);
        std::wstring pattern = vDataToWString(args[1]);

        size_t pos = str.find(pattern);
        if (pos == std::wstring::npos) return { -1LL };

        return { static_cast<long long>(pos) };
        };

    // --- SUBSTR(str, start, [length]) ---
    m_functionsHandlers[L"SUBSTR"] = [this](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return args.empty() ? vData{ L"" } : args[0];

        std::wstring str = vDataToWString(args[0]);
        int start = static_cast<int>(vDataToDouble(args[1]));

        // Validare start bounds
        if (start < 0) start = 0;
        if (start >= (int)str.length()) return { L"" };

        if (args.size() >= 3) {
            int len = static_cast<int>(vDataToDouble(args[2]));
            if (len < 0) return { L"" };
            return { str.substr(start, len) };
        }

        return { str.substr(start) };
        };

    // functii mate
// --- ABS(x) -> Valoarea absolută ---
    m_functionsHandlers[L"ABS"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) return { 0.0 };
        return { std::fabs(this->vDataToDouble(args[0])) };
        };

    // --- ROUND(x) -> Rotunjire la cel mai apropiat întreg ---
    m_functionsHandlers[L"ROUND"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) return { 0.0 };
        return { std::round(this->vDataToDouble(args[0])) };
        };

    // --- FLOOR(x) -> Cel mai mare întreg mai mic sau egal cu x ---
    m_functionsHandlers[L"FLOOR"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) return { 0.0 };
        return { std::floor(this->vDataToDouble(args[0])) };
        };

    // --- CEIL(x) -> Cel mai mic întreg mai mare sau egal cu x ---
    m_functionsHandlers[L"CEIL"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) return { 0.0 };
        return { std::ceil(this->vDataToDouble(args[0])) };
        };

    // --- MIN(a, b) -> Returnează valoarea minimă ---
    m_functionsHandlers[L"MIN"] = [this](const std::vector<vData>& args) -> vData {
        // Dacă nu avem 2 argumente, returnăm primul argument sau 0.0
        if (args.size() < 2) return args.empty() ? vData{ 0.0 } : args[0];

        double a = this->vDataToDouble(args[0]);
        double b = this->vDataToDouble(args[1]);
        return { std::min<double>(a, b) };
        };

    // --- MAX(a, b) -> Returnează valoarea maximă ---
    m_functionsHandlers[L"MAX"] = [this](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return args.empty() ? vData{ 0.0 } : args[0];

        double a = this->vDataToDouble(args[0]);
        double b = this->vDataToDouble(args[1]);
        return { std::max<double>(a, b) };
        };


    m_functionsHandlers[L"READFILE"] = [this](const auto& args) {return this->handleReadFileFunc(args); };
    m_functionsHandlers[L"WRITEFILE"] = [this](const auto& args) {return this->handleWriteFileFunc(args); };
    m_functionsHandlers[L"APPENDFILE"] = [this](const auto& args) {return this->handleAppendFileFunc(args); };
    m_functionsHandlers[L"EXISTSFILE"] = [this](const auto& args) {return this->handleExistsFileFunc(args); };
    m_functionsHandlers[L"DELETEFILE"] = [this](const auto& args) {return this->handleDeleteFileFunc(args); };

    // Prototip conceptual pentru handlere
    m_functionsHandlers[L"JSON_ENCODE"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(L"null");

        // Funcție lambda internă pentru recursivitate
        std::function<std::wstring(const vData&)> encode = [&](const vData& d) -> std::wstring {
            // 1. Rezolvăm pointerii (dacă d e pointer, mergem la valoare)
            const vData* current = &d;
            if (std::holds_alternative<vData*>(current->value)) {
                vData* ptr = std::get<vData*>(current->value);
                if (ptr) current = ptr;
            }

            // 2. Switch pe tipul de date
            if (current->isString()) return L"\"" + std::get<std::wstring>(current->value) + L"\"";
            if (current->isInt())    return std::to_wstring(std::get<long long>(current->value));
            if (current->isFloat()) {
                std::wstring s = std::to_wstring(std::get<double>(current->value));
                s.erase(s.find_last_not_of(L'0') + 1, std::string::npos);
                if (s.back() == L'.') s.pop_back();
                return s;
            }
            if (current->isBool())   return std::get<bool>(current->value) ? L"true" : L"false";

            if (current->isArray()) {
                auto arr = std::get<vDataArray>(current->value);
                std::wstring res = L"[";
                for (size_t i = 0; i < arr->size(); ++i) {
                    res += encode((*arr)[i]) + (i == arr->size() - 1 ? L"" : L",");
                }
                return res + L"]";
            }

            if (current->isMap()) {
                auto m = std::get<vDataMap>(current->value);
                std::wstring res = L"{";
                size_t i = 0;
                for (auto const& [key, val] : *m) {
                    res += L"\"" + key + L"\":" + encode(val) + (i == m->size() - 1 ? L"" : L",");
                    i++;
                }
                return res + L"}";
            }

            return L"null";
            };

        return vData(encode(args[0]));
        };

    m_functionsHandlers[L"JSON_DECODE"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty() || !args[0].isString()) return vData(std::monostate{});

        std::wstring rawJson = std::get<std::wstring>(args[0].value);

        // 1. Curățăm spațiile de la margini
        std::wstring cleanJson = trim(rawJson);

        // 2. IMPORTANT: Dacă string-ul a venit „împachetat” în ghilimele de la Shell, 
        // le scoatem, altfel parserul îl va vedea ca pe un literal string, nu ca pe un obiect.
        if (cleanJson.size() >= 2 && cleanJson.front() == L'"' && cleanJson.back() == L'"') {
            cleanJson = cleanJson.substr(1, cleanJson.size() - 2);
        }

        // 3. Unescape (pentru a transforma \" în ")
        cleanJson = unescape(cleanJson);

        size_t pos = 0;
        try {
            return parseJSONValue(cleanJson, pos);
        }
        catch (...) {
            return vData(std::monostate{});
        }
        };

    // --- NOW() -> Returnează Unix Timestamp (secunde de la 1970) ---
    m_functionsHandlers[L"NOW"] = [this](const std::vector<vData>& args) -> vData {
        auto acum = std::chrono::system_clock::now();
        auto secunde = std::chrono::duration_cast<std::chrono::seconds>(acum.time_since_epoch()).count();
        return vData((long long)secunde);
        };

    // --- DATE() ---
    m_functionsHandlers[L"DATE"] = [this](const std::vector<vData>& args) -> vData {
        return vData(PortTools::getFormattedTime(L"%Y-%m-%d"));
        };

    // --- TIME() ---
    m_functionsHandlers[L"TIME"] = [this](const std::vector<vData>& args) -> vData {
        return vData(PortTools::getFormattedTime(L"%H:%M:%S"));
        };

    m_functionsHandlers[L"MSTIME"] = [this](const std::vector<vData>& args) -> vData {
        auto acum = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(acum.time_since_epoch()).count();
        return vData((long long)ms);
        };

    m_functionsHandlers[L"CHR"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData{ L"" };

        const vData& input = args[0];
        long long code = 0;

        // 1. Extragerea valorii numerice (indiferent dacă e Int sau Float)
        if (input.isInt()) {
            code = std::get<long long>(input.value);
        }
        else if (input.isFloat()) {
            code = static_cast<long long>(std::get<double>(input.value));
        }
        else if (input.isString()) {
            try {
                code = std::stoll(std::get<std::wstring>(input.value));
            }
            catch (...) {
                return vData{ L"" };
            }
        }

        // 2. Conversia în caracter (folosind wchar_t pentru consistență cu Oli)
        // Ne asigurăm că valoarea este într-un interval rezonabil
        if (code < 0) return vData{ L"" };

        wchar_t ch = static_cast<wchar_t>(code);

        // Returnăm un wstring format dintr-un singur caracter
        return vData{ std::wstring(1, ch) };
        };

    // Mapăm și sub numele de CHAR pentru prietenie cu alte limbaje
    m_functionsHandlers[L"CHAR"] = m_functionsHandlers[L"CHR"];

    m_functionsHandlers[L"ASC"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty() || !args[0].isString()) return vData{ 0LL };

        const std::wstring& str = std::get<std::wstring>(args[0].value);
        if (str.empty()) return vData{ 0LL };

        return vData{ static_cast<long long>(str[0]) };
        };

    m_functionsHandlers[L"WRITE"] = [this](const std::vector<vData>& args) -> vData {
        if (args.empty()) return { 0LL };

        std::wstring str = vDataToWString(args[0]);
        std::wcout << str; // FĂRĂ std::endl
        std::wcout.flush(); // Ne asigurăm că apare pe ecran imediat

        return { 0LL };
        };
}


vData vOliEngine::handleInputFunc(const std::vector<vData>& args) {
    // 1. Afișăm prompt-ul (dacă există)
    if (!args.empty()) {
        std::wcout << vDataToWString(args[0]);
    }

    // 2. Citim linia de la utilizator
    std::wstring userInput;
    // Folosim getline pentru a permite spații în input
    if (!std::getline(std::wcin, userInput)) {
        return { L"" }; // Returnăm string gol în caz de EOF sau eroare
    }

    // 3. (Opțional) Putem încerca să detectăm dacă input-ul este număr
    // Pentru simplitate, momentan îl returnăm ca String. 
    // Utilizatorul poate face operații matematice oricum datorită vDataToDouble-ului tău.
    return { userInput };
}

vData vOliEngine::handleRandomFunc(const std::vector<vData>& args) {
    long long min = 0, max = 100;
    if (args.size() >= 2) {
        min = vDataToLong(args[0]);
        max = vDataToLong(args[1]);
    }

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<long long> dis(min, max);

    return { dis(gen) };
}

vData vOliEngine::handleWaitFunc(const std::vector<vData>& args) {
    if (!args.empty()) {
        long long ms = vDataToLong(args[0]);
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    return { std::monostate{} };
}

vData vOliEngine::handleSysFunc(const std::vector<vData>& args) {
    if (args.empty()) return { L"" };
    std::wstring command = vDataToWString(args[0]);

    // Curățare ghilimele...
    if (command.size() >= 2 && command.front() == L'"' && command.back() == L'"') {
        command = command.substr(1, command.size() - 2);
    }

    std::wstring output;
    std::wstring line;
    FILE* pipe = PortTools::openPipe(command, L"r");

    if (!pipe) return { L"ERROR" };

    // Citire abstractizată - zero platform-specific code aici
    while (PortTools::readLineFromPipe(pipe, line)) {
        output += line;
    }

    PortTools::closePipe(pipe);
    return vData(output);
}

vData vOliEngine::handleContainsFunc(const std::vector<vData>& args) {
    // Avem nevoie de exact 2 argumente: substring și sursă
    if (args.size() < 2) {
        LOG_ERROR(L"CONTAINS requires 2 arguments: (substring, source_text)");
        return vData(false);
    }

    // 1. Extragem datele folosind vDataToWString pentru a suporta orice tip (chiar și INT convertit)
    std::wstring toFind = vDataToWString(args[0]);
    std::wstring source = vDataToWString(args[1]);

    // 2. Funcție lambda rapidă pentru a elimina ghilimelele de la exterior ("text" -> text)
    auto stripQuotes = [](std::wstring& s) {
        if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"') {
            s = s.substr(1, s.size() - 2);
        }
        };

    stripQuotes(toFind);
    stripQuotes(source);

    // 3. Executăm căutarea
    // std::wstring::npos înseamnă că substring-ul NU a fost găsit
    bool found = (source.find(toFind) != std::wstring::npos);

    //LOG_DEBUG(L"CONTAINS: Checking if '" + toFind + L"' is in source... " + (found ? L"Found" : L"Not Found"));

    return vData(found);
}

vData vOliEngine::handleEvalFunc(const std::vector<vData>& args) {
    if (args.empty()) return { 0LL }; // Sau std::monostate{} pentru NULL

    // 1. Dacă nu e string, returnăm valoarea pură (ex: EVAL(5) -> 5)
    if (!std::holds_alternative<std::wstring>(args[0].value)) {
        return args[0];
    }

    std::wstring expr = std::get<std::wstring>(args[0].value);

    // Curățăm spațiile inutile de la început/final
    expr = trim(expr);

    if (expr.empty()) return { std::monostate{} };

    try {
        // 2. Evaluarea propriu-zisă
        // IMPORTANT: Asigură-te că evaluateExpression folosește scope-ul curent!
        vData result = evaluateExpression(expr);
        return result;
    }
    catch (const std::exception& e) {
        std::wstring errorMsg = PortTools::utf8_to_wstring(e.what());
        LOG_ERROR(L"[EVAL ERROR] Failed to evaluate: '" + expr + L"'. Details: " + errorMsg);
        return { std::monostate{} }; // Returnăm NULL în caz de eroare de sintaxă
    }
    catch (...) {
        LOG_ERROR(L"[EVAL ERROR] Critical failure in EVAL of: " + expr);
        return { std::monostate{} };
    }
}

vData vOliEngine::handleIntFunc(const std::vector<vData>& args) {
    if (args.empty()) return vData{ 0LL }; // Default la 0 de tip long long

    const vData& input = args[0];

    // Dacă este deja Int, returnăm copia
    if (input.isInt()) return input;

    // Dacă este Float, facem cast la long long (std::get<double>)
    if (input.isFloat()) {
        return vData{ static_cast<long long>(std::get<double>(input.value)) };
    }

    // Dacă este String, încercăm conversia numerică
    if (input.isString()) {
        try {
            return vData{ std::stoll(std::get<std::wstring>(input.value)) };
        }
        catch (...) {
            return vData{ 0LL }; // Conversie eșuată
        }
    }

    return vData{ 0LL };
}

vData vOliEngine::handleFloatFunc(const std::vector<vData>& args) {
    if (args.empty()) return vData{ 0.0 };

    const vData& input = args[0];

    // 1. Deja Float -> returnăm identic
    if (input.isFloat()) return input;

    // 2. Int -> Double
    if (input.isInt()) {
        return vData{ static_cast<double>(std::get<long long>(input.value)) };
    }

    // 3. String -> Double
    if (input.isString()) {
        try {
            // std::stod se ocupă de conversia wstring -> double
            return vData{ std::stod(std::get<std::wstring>(input.value)) };
        }
        catch (...) {
            return vData{ 0.0 }; // Fallback la eroare
        }
    }

    // 4. Bool -> Double (1.0 sau 0.0)
    if (input.isBool()) {
        return vData{ std::get<bool>(input.value) ? 1.0 : 0.0 };
    }

    return vData{ 0.0 };
}

vData vOliEngine::handleStrFunc(const std::vector<vData>& args) {
    if (args.empty()) return vData{ L"" }; // Default: șir vid

    const vData& input = args[0];

    // 1. Dacă este deja String, returnăm o copie
    if (input.isString()) return input;

    // 2. Pentru restul tipurilor (Int, Float, Bool, Array, Map)
    // Folosim funcția ta vDataToWString care se ocupă deja de formatare
    return vData{ vDataToWString(input) };
}


vData vOliEngine::handleArrayFunc(const std::vector<vData>& args) {
    // Creăm un shared_ptr care copiază conținutul lui 'args' direct în heap
    return vData{ std::make_shared<std::vector<vData>>(args) };
}

vData vOliEngine::handleMapFunc(const std::vector<vData>& args) {
    // 1. Alocăm Map-ul în Heap și obținem pointerul partajat
    // Folosim std::make_shared pentru eficiență
    vDataMap newMap = std::make_shared<std::map<std::wstring, vData>>();

    // 2. Dacă nu avem argumente, returnăm shared_ptr-ul către map-ul gol
    if (args.empty()) {
        return vData{ newMap };
    }

    // 3. Parcurgem argumentele doi câte doi (cheie, valoare)
    for (size_t i = 0; i + 1 < args.size(); i += 2) {
        std::wstring key;

        // Folosim utilitarul tău vDataToWString pentru a garanta că avem un string
        key = vDataToWString(args[i]);

        // 4. OPERAȚIA CRITICĂ: Dereferențiem pointerul (*) pentru a folosi []
        // Punem paranteze în jurul dereferențierii pentru prioritate: (*newMap)
        (*newMap)[key] = args[i + 1];
    }

    if (args.size() % 2 != 0) {
        // LOG_WARNING(L"MAP() ignored odd last argument.");
    }

    // 5. Returnăm vData care ambalează pointerul nostru
    return vData{ newMap };
}

vData vOliEngine::handleSplitFunc(const std::vector<vData>& args) {
    // 1. Returnăm un Array gol (inițializat corect în heap)
    if (args.empty()) {
        return vData{ std::make_shared<std::vector<vData>>() };
    }

    std::wstring text = vDataToWString(args[0]);
    std::wstring delims = (args.size() > 1) ? vDataToWString(args[1]) : L" ";

    // Curățăm secvențele de escape (rămâne neschimbat)
    if (delims == L"\\n") delims = L"\n";
    else if (delims == L"\\r\\n") delims = L"\r\n";
    else if (delims == L"\\t") delims = L"\t";

    // 2. Alocăm vectorul în HEAP
    auto result = std::make_shared<std::vector<vData>>();

    size_t lastPos = 0;
    size_t pos = text.find(delims);
    bool isWhitespaceSplit = (delims == L" ");

    while (pos != std::wstring::npos) {
        std::wstring token = text.substr(lastPos, pos - lastPos);

        if (!isWhitespaceSplit || !token.empty()) {
            // 3. Folosim -> pentru a accesa push_back pe vectorul din heap
            result->push_back(vData{ token });
        }

        lastPos = pos + delims.length();
        pos = text.find(delims, lastPos);
    }

    std::wstring lastToken = text.substr(lastPos);
    if (!isWhitespaceSplit || !lastToken.empty()) {
        result->push_back(vData{ lastToken });
    }

    // 4. Returnăm shared_ptr-ul
    return vData{ result };
}

vData vOliEngine::handleJoinFunc(const std::vector<vData>& args) {
    // 1. Validare: Avem nevoie de cel puțin un Array
    if (args.empty() || !args[0].isArray()) {
        return vData{ L"" };
    }

    // 2. Extragem shared_ptr-ul către vector
    const auto& listPtr = std::get<vDataArray>(args[0].value);

    // Verificăm dacă pointerul este valid (nu e null)
    if (!listPtr) return vData{ L"" };

    // 3. Separatorul (rămâne neschimbat)
    std::wstring separator = (args.size() > 1) ? vDataToWString(args[1]) : L" ";

    std::wstring result;

    // 4. Folosim ->size() pentru a accesa vectorul de pe heap
    for (size_t i = 0; i < listPtr->size(); ++i) {
        // 5. Dereferențiem (*listPtr)[i] pentru a ajunge la elementul vData
        result += vDataToWString((*listPtr)[i]);

        if (i < listPtr->size() - 1) {
            result += separator;
        }
    }

    return vData{ result };
}

vData vOliEngine::handleTrimFunc(const std::vector<vData>& args) {
    if (args.empty()) return vData{ L"" };

    // Convertim argumentul în string și îi aplicăm funcția de curățare
    std::wstring text = vDataToWString(args[0]);
    return vData{ trim(text) };
}


vData vOliEngine::handleReadFileFunc(const std::vector<vData>& args) {
    if (args.empty() || !args[0].isString()) {
        LOG_ERROR(L"[RUNTIME ERROR] readfile() requires a path string.");
        return vData(std::monostate{}); // Returnăm NULL, nu string gol
    }

    std::wstring pathW = std::get<std::wstring>(args[0].value);
    try {
        std::string utf8_path = PortTools::wstring_to_utf8(pathW);
        std::ifstream file(utf8_path, std::ios::binary);

        if (!file.is_open()) {
            // Nu logăm eroare neapărat, lăsăm scriptul să decidă ce face cu NULL-ul
            return vData(std::monostate{});
        }

        // Citire eficientă
        std::string buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        std::wstring wcontent = PortTools::utf8_to_wstring(buffer);

        // Eliminare BOM (Byte Order Mark)
        if (!wcontent.empty() && (unsigned short)wcontent[0] == 0xFEFF) {
            wcontent.erase(0, 1);
        }

        return vData(wcontent);
    }
    catch (...) {
        LOG_ERROR(L"[RUNTIME ERROR] Exception reading file: " + pathW);
        return vData(std::monostate{});
    }
}

vData vOliEngine::handleWriteFileFunc(const std::vector<vData>& args) {
    // Validare: avem nevoie de cel puțin path (string) și ceva de scris (orice)
    if (args.size() < 2 || !args[0].isString()) {
        LOG_ERROR(L"[RUNTIME ERROR] writefile() requires (path, content).");
        return vData(0LL);
    }

    std::wstring pathW = std::get<std::wstring>(args[0].value);

    // REPARAT: Convertim ORICE tip de dată în string (Map, Array, Number)
    // Folosim helper-ul tău vDataToWString sau o conversie similară
    std::wstring contentW = vDataToWString(args[1]);

    contentW = PortTools::normalize_newlines_for_write(contentW);

    try {
        std::string path = PortTools::wstring_to_utf8(pathW);
        std::string content = PortTools::wstring_to_utf8(contentW);

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            LOG_ERROR(L"[RUNTIME ERROR] Cannot open for writing: " + pathW);
            return vData(0LL);
        }

        file.write(content.data(), content.size());
        file.close();

        return vData(1LL); // Succes (long long)
    }
    catch (...) {
        LOG_ERROR(L"[RUNTIME ERROR] Exception writing file: " + pathW);
        return vData(0LL);
    }
}

vData vOliEngine::handleAppendFileFunc(const std::vector<vData>& args) {
    vData result;

    // 1. Validare argumente
    if (args.size() != 2 ||
        !std::holds_alternative<std::wstring>(args[0].value) ||
        !std::holds_alternative<std::wstring>(args[1].value))
    {
        LOG_ERROR(L"[RUNTIME ERROR] appendfile() requires (path, content).");
        result.value = 0;
        return result;
    }

    std::wstring pathW = std::get<std::wstring>(args[0].value);
    std::wstring contentW = std::get<std::wstring>(args[1].value);

    // Normalizează newline-urile în funcție de OS
    contentW = PortTools::normalize_newlines_for_write(contentW);

    try {
        // 2. Convertim wide → UTF-8 pentru Linux
        std::string path = PortTools::wstring_to_utf8(pathW);
        std::string content = PortTools::wstring_to_utf8(contentW);

        // 3. Deschidem fișierul în modul append
        std::ofstream file(path, std::ios::binary | std::ios::app);
        if (!file.is_open()) {
            LOG_ERROR(L"[RUNTIME ERROR] Cannot append to file: " + pathW);
            result.value = 0;
            return result;
        }

        // 4. Scriem la final
        file.write(content.data(), content.size());
        file.close();

        result.value = 1; // succes
        return result;
    }
    catch (...) {
        LOG_ERROR(L"[RUNTIME ERROR] Unknown error in appendfile().");
    }

    result.value = 0;
    return result;
}


vData vOliEngine::handleExistsFileFunc(const std::vector<vData>& args) {
    vData result;

    // 1. Validare argumente
    if (args.size() != 1 ||
        !std::holds_alternative<std::wstring>(args[0].value))
    {
        LOG_ERROR(L"[RUNTIME ERROR] existsfile() requires a single string path.");
        result.value = 0;
        return result;
    }

    std::wstring pathW = std::get<std::wstring>(args[0].value);

    try {
        // 2. Convertim wide → UTF-8 pentru Linux
        std::string path = PortTools::wstring_to_utf8(pathW);

        // 3. Verificăm existența
        bool ok = std::filesystem::exists(path);

        result.value = ok ? 1 : 0;
        return result;
    }
    catch (...) {
        LOG_ERROR(L"[RUNTIME ERROR] existsfile() failed for: " + pathW);
    }

    result.value = 0;
    return result;
}

vData vOliEngine::handleDeleteFileFunc(const std::vector<vData>& args) {
    vData result;

    // 1. Validare argumente
    if (args.size() != 1 ||
        !std::holds_alternative<std::wstring>(args[0].value))
    {
        LOG_ERROR(L"[RUNTIME ERROR] deletefile() requires a single string path.");
        result.value = 0;
        return result;
    }

    std::wstring pathW = std::get<std::wstring>(args[0].value);

    try {
        // 2. Convertim wide → UTF-8 pentru Linux
        std::string path = PortTools::wstring_to_utf8(pathW);

        // 3. Încercăm să ștergem fișierul
        bool ok = false;

        if (std::filesystem::exists(path) &&
            std::filesystem::is_regular_file(path))
        {
            ok = std::filesystem::remove(path);
        }

        result.value = ok ? 1 : 0;
        return result;
    }
    catch (...) {
        LOG_ERROR(L"[RUNTIME ERROR] deletefile() failed for: " + pathW);
    }

    result.value = 0;
    return result;
}

vData vOliEngine::parseJSONValue(const std::wstring& json, size_t& pos) {
    auto skipWhitespace = [&]() {
        while (pos < json.length() && std::iswspace(json[pos])) pos++;
        };

    skipWhitespace();
    if (pos >= json.length()) return vData(std::monostate{});

    wchar_t c = json[pos];

    // --- CASE: OBJECT { ... } ---
    if (c == L'{') {
        pos++; // Consumăm '{'
        vData mapObj = vData::CreateMap();
        auto mapPtr = std::get<vDataMap>(mapObj.value);

        skipWhitespace();
        if (pos < json.length() && json[pos] == L'}') {
            pos++; // Obiect gol
            return mapObj;
        }

        while (pos < json.length()) {
            skipWhitespace();
            // În JSON, cheia TREBUIE să fie un string
            vData keyData = parseJSONValue(json, pos);
            std::wstring key = keyData.isString() ? std::get<std::wstring>(keyData.value) : L"";

            skipWhitespace();
            if (pos < json.length() && json[pos] == L':') {
                pos++; // Consumăm separatorul ':'
            }

            // Citim valoarea asociată cheii
            (*mapPtr)[key] = parseJSONValue(json, pos);

            skipWhitespace();
            if (pos < json.length() && json[pos] == L',') {
                pos++; // Consumăm virgula și continuăm bucla
                // Verificăm dacă urmează direct închiderea (trailing comma)
                skipWhitespace();
                if (pos < json.length() && json[pos] == L'}') {
                    pos++; break;
                }
            }
            else if (pos < json.length() && json[pos] == L'}') {
                pos++; // Am ajuns la finalul obiectului
                break;
            }
            else {
                // Eroare de sintaxă (lipsă virgulă sau închidere)
                break;
            }
        }
        return mapObj;
    }

    // --- CASE: ARRAY [ ... ] ---
    else if (c == L'[') {
        pos++; // Consumăm '['
        vData arrObj = vData::CreateArray();
        auto arrPtr = arrObj.rawArray();

        skipWhitespace();
        if (pos < json.length() && json[pos] == L']') {
            pos++; // Array gol
            return arrObj;
        }

        while (pos < json.length()) {
            arrPtr->push_back(parseJSONValue(json, pos));

            skipWhitespace();
            if (pos < json.length() && json[pos] == L',') {
                pos++; // Consumăm virgula
                skipWhitespace();
                if (pos < json.length() && json[pos] == L']') {
                    pos++; break; // Trailing comma
                }
            }
            else if (pos < json.length() && json[pos] == L']') {
                pos++; // Final de array
                break;
            }
            else {
                break;
            }
        }
        return arrObj;
    }

    // --- CASE: STRING " ... " ---
    else if (c == L'"') {
        pos++; // Consumăm ghilimeaua de deschidere
        std::wstring s;
        while (pos < json.length() && json[pos] != L'"') {
            if (json[pos] == L'\\') {
                pos++;
                if (pos >= json.length()) break;
                wchar_t esc = json[pos++];
                switch (esc) {
                case L'n': s += L'\n'; break;
                case L't': s += L'\t'; break;
                case L'r': s += L'\r'; break;
                case L'\\': s += L'\\'; break;
                case L'"': s += L'"'; break;
                case L'b': s += L'\b'; break;
                case L'f': s += L'\f'; break;
                case L'/': s += L'/'; break;
                default: s += esc; break;
                }
            }
            else {
                s += json[pos++];
            }
        }
        if (pos < json.length() && json[pos] == L'"') pos++; // Consumăm ghilimeaua de închidere
        return vData(s);
    }

    // --- CASE: BOOLEAN / NULL (Folosim o metodă sigură de potrivire) ---
    auto match = [&](const std::wstring& word) -> bool {
        if (pos + word.length() <= json.length() && json.compare(pos, word.length(), word) == 0) {
            pos += word.length();
            return true;
        }
        return false;
        };

    if (match(L"true"))  return vData(true);
    if (match(L"false")) return vData(false);
    if (match(L"null"))  return vData(std::monostate{});

    // --- CASE: NUMBER ---
    if (std::iswdigit(c) || c == L'-') {
        size_t start = pos;
        bool isFloat = false;
        // Extindem setul de caractere pentru numere (inclusiv notația științifică)
        while (pos < json.length() && (std::iswdigit(json[pos]) || json[pos] == L'.' ||
            json[pos] == L'-' || json[pos] == L'e' || json[pos] == L'E' || json[pos] == L'+')) {
            if (json[pos] == L'.') isFloat = true;
            pos++;
        }
        std::wstring numStr = json.substr(start, pos - start);
        try {
            if (isFloat) return vData(std::stod(numStr));
            return vData(std::stoll(numStr));
        }
        catch (...) { return vData(0LL); }
    }

    // Fallback de siguranță pentru a preveni buclele infinite
    pos++;
    return vData(std::monostate{});
}