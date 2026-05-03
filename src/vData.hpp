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
};

#endif