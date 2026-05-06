#ifndef VDATA_HPP
#define VDATA_HPP

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <cwctype>
#include <cctype>
#include <map>
#include <functional>
#include <variant>


#include "OliCommandParser.hpp"
struct vData; // Forward declaration

//using vDataArray = std::vector<vData>;
//using vDataMap = std::map<std::wstring, vData>;

using vDataArray = std::shared_ptr<std::vector<vData>>;
using vDataMap = std::shared_ptr<std::unordered_map<std::wstring, vData>>;
//using vDataMap = std::shared_ptr<std::map<std::wstring, vData>>;

using OliCommandHandler = std::function<void(const ShellCommand&)>;
using OliFunctionHandler = std::function<vData(const std::vector<vData>& args)>;

using vDataValue = std::variant<
    std::monostate,
    std::wstring,
    long long,
    double,
    bool,
    vDataArray,
    vDataMap,
    vData* // <--- Noul tip: Pointer către o altă vData
>;

struct vData {
    vDataValue value;

    // --- LOGICA DE DEREFERENȚIERE (Inima structurii) ---
    // Returnează referința către datele reale, trecând prin pointeri dacă e cazul
    vData& getTrueData() {
        if (std::holds_alternative<vData*>(value)) {
            vData* ptr = std::get<vData*>(value);
            return ptr ? ptr->getTrueData() : *this;
        }
        return *this;
    }

    const vData& getTrueData() const {
        if (std::holds_alternative<vData*>(value)) {
            vData* ptr = std::get<vData*>(value);
            return ptr ? ptr->getTrueData() : *this;
        }
        return *this;
    }
    /*
    // --- HELPERI PENTRU ACCES RAW ---
    std::vector<vData>* rawArray() {
        auto& trueData = getTrueData();
        if (!trueData.isArray()) return nullptr;
        return std::get<vDataArray>(trueData.value).get();
    }

    std::unordered_map<std::wstring, vData>* rawMap() {
        auto& trueData = getTrueData();
        if (!trueData.isMap()) return nullptr;
        return std::get<vDataMap>(trueData.value).get();
    }
    */
    // --- FACTORY METHODS ---
    static vData CreateMap() {
        //return vData{ std::make_shared<std::map<std::wstring, vData>>() };
        return vData{ std::make_shared<std::unordered_map<std::wstring, vData>>() };
    }

    static vData CreateArray() {
        return vData{ std::make_shared<std::vector<vData>>() };
    }

    // --- VERIFICĂRI DE TIP (Acum funcționează și pentru pointeri) ---
    bool isArray()  const { return std::holds_alternative<vDataArray>(getTrueData().value); }
    bool isMap()    const { return std::holds_alternative<vDataMap>(getTrueData().value); }
    bool isString() const { return std::holds_alternative<std::wstring>(getTrueData().value); }
    bool isInt()    const { return std::holds_alternative<long long>(getTrueData().value); }
    bool isFloat()  const { return std::holds_alternative<double>(getTrueData().value); }
    bool isBool()   const { return std::holds_alternative<bool>(getTrueData().value); }
    bool isNull()   const { return std::holds_alternative<std::monostate>(getTrueData().value); }
    bool isPointer() const { return std::holds_alternative<vData*>(value); }

    // --- OPERATORI ---
    bool operator==(const vData& other) const {
        const vData& t1 = this->getTrueData();
        const vData& t2 = other.getTrueData();

        // Dacă tipurile de bază după dereferențiere sunt diferite, nu sunt egale
        if (t1.value.index() != t2.value.index()) return false;

        // Comparăm valorile reale
        return t1.value == t2.value;
    }

    long long toInt() const {
        if (std::holds_alternative<long long>(value)) return std::get<long long>(value);
        if (std::holds_alternative<double>(value)) return (long long)std::get<double>(value);
        return 0;
    }

    std::wstring toWString() const {
        const vData& actual = getTrueData();

        if (actual.isNull())   return L"null";
        if (actual.isString()) return std::get<std::wstring>(actual.value);

        // Formatare inteligentă pentru numere
        if (actual.isInt())    return std::to_wstring(std::get<long long>(actual.value));
        if (actual.isFloat()) {
            double d = std::get<double>(actual.value);
            // Dacă e practic întreg (ex: 10.0), scoatem zecimalele
            if (d == (long long)d) return std::to_wstring((long long)d);
            std::wstringstream wss;
            wss << d; // wstringstream e mai curat decât to_wstring pt double
            return wss.str();
        }

        if (actual.isBool()) return std::get<bool>(actual.value) ? L"true" : L"false";

        if (actual.isArray()) {
            auto arr = std::get<vDataArray>(actual.value);
            std::wstring res = L"[";
            for (size_t i = 0; i < arr->size(); ++i) {
                res += (*arr)[i].toWString();
                if (i < arr->size() - 1) res += L", ";
            }
            return res + L"]";
        }

        if (actual.isMap()) {
            auto m = std::get<vDataMap>(actual.value);
            std::wstring res = L"{";
            size_t i = 0;
            for (auto const& [key, val] : *m) {
                res += key + L": " + val.toWString();
                if (++i < m->size()) res += L", ";
            }
            return res + L"}";
        }

        return L"unknown";
    }

    std::vector<vData>* rawArray() {
        auto& trueData = getTrueData();
        if (!trueData.isArray()) return nullptr;
        return std::get<vDataArray>(trueData.value).get();
    }

    std::unordered_map<std::wstring, vData>* rawMap() {
        auto& trueData = getTrueData();
        if (!trueData.isMap()) return nullptr;
        return std::get<vDataMap>(trueData.value).get();
    }

    // --- Variantele noi (CONST) ---
    // Atenție la "const" de la finalul liniei!
    const std::vector<vData>* rawArray() const {
        const auto& trueData = getTrueData();
        if (!trueData.isArray()) return nullptr;
        return std::get<vDataArray>(trueData.value).get();
    }

    const std::unordered_map<std::wstring, vData>* rawMap() const {
        const auto& trueData = getTrueData();
        if (!trueData.isMap()) return nullptr;
        return std::get<vDataMap>(trueData.value).get();
    }

    std::wstring toPlainString() const {
        const vData& actual = getTrueData();
        if (actual.isMap()) {
            auto* m = actual.getTrueData().rawMap();
            // Dacă map-ul are un singur element, probabil e valoarea variabilei
            if (m && m->size() == 1) return m->begin()->second.getTrueData().toWString();
        }
        return actual.toWString();
    }

    vData getScalarValue() const {
        const vData& actual = getTrueData();

        // Dacă este un Map (ierarhie) și are elemente, încercăm să extragem 
        // valoarea "frunză" (ultima valoare din ierarhie)
        if (actual.isMap()) {
            auto* m = actual.rawMap();

            if (m && m->size() == 1) {
                // Returnăm valoarea primului (și singurului) element din ierarhie
                return m->begin()->second.getScalarValue();
            }
        }
        return actual;
    }

    vData getFlattenedValue() const {
        // Dacă e un Map (ierarhie), săpăm în el
        if (std::holds_alternative<vDataMap>(value)) {
            auto* m = rawMap();
            if (m && !m->empty()) return m->begin()->second.getFlattenedValue();
        }
        // Returnăm valoarea așa cum e (chiar dacă e vData*)
        return *this;
    }
};

#endif