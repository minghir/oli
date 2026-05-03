#ifndef VDATASERIALIZE_HPP
#define VDATASERIALIZE_HPP

#include "vData.hpp"
#include <iostream>
#include <variant>

// Enum pentru identificarea tipului în fișierul binar
enum class vDataType : uint8_t {
    NIL = 0,
    INT = 1,
    DOUBLE = 2,
    STRING = 3,
    BOOL = 4
};

/**
 * Scrie un obiect vData în fluxul binar.
 * Folosește getTrueData() pentru a rezolva eventualii pointeri înainte de salvare.
 */
inline void serializevData(const vData& data, std::ostream& out) {
    const vData& actual = data.getTrueData();

    if (actual.isNull()) {
        uint8_t type = (uint8_t)vDataType::NIL;
        out.write(reinterpret_cast<const char*>(&type), 1);
    }
    else if (actual.isInt()) {
        uint8_t type = (uint8_t)vDataType::INT;
        out.write(reinterpret_cast<const char*>(&type), 1);
        long long val = std::get<long long>(actual.value);
        out.write(reinterpret_cast<const char*>(&val), sizeof(val));
    }
    else if (actual.isFloat()) {
        uint8_t type = (uint8_t)vDataType::DOUBLE;
        out.write(reinterpret_cast<const char*>(&type), 1);
        double val = std::get<double>(actual.value);
        out.write(reinterpret_cast<const char*>(&val), sizeof(val));
    }
    else if (actual.isBool()) {
        uint8_t type = (uint8_t)vDataType::BOOL;
        out.write(reinterpret_cast<const char*>(&type), 1);
        bool val = std::get<bool>(actual.value);
        out.write(reinterpret_cast<const char*>(&val), sizeof(val));
    }
    else if (actual.isString()) {
        uint8_t type = (uint8_t)vDataType::STRING;
        out.write(reinterpret_cast<const char*>(&type), 1);
        const std::wstring& str = std::get<std::wstring>(actual.value);
        uint32_t len = static_cast<uint32_t>(str.size());

        // Salvăm lungimea (număr de caractere)
        out.write(reinterpret_cast<const char*>(&len), sizeof(len));
        // Salvăm conținutul brut (bytes = len * mărime_wchar)
        if (len > 0) {
            out.write(reinterpret_cast<const char*>(str.data()), len * sizeof(wchar_t));
        }
    }
}

/**
 * Reconstruiește un obiect vData din fluxul binar.
 */
inline vData deserializevData(std::istream& in) {
    uint8_t typeRaw;
    if (!in.read(reinterpret_cast<char*>(&typeRaw), 1)) {
        return vData{ std::monostate{} };
    }

    vDataType type = static_cast<vDataType>(typeRaw);

    switch (type) {
    case vDataType::INT: {
        long long val;
        in.read(reinterpret_cast<char*>(&val), sizeof(val));
        return vData{ val };
    }
    case vDataType::DOUBLE: {
        double val;
        in.read(reinterpret_cast<char*>(&val), sizeof(val));
        return vData{ val };
    }
    case vDataType::BOOL: {
        bool val;
        in.read(reinterpret_cast<char*>(&val), sizeof(val));
        return vData{ val };
    }
    case vDataType::STRING: {
        uint32_t len;
        in.read(reinterpret_cast<char*>(&len), sizeof(len));
        if (len == 0) return vData{ std::wstring(L"") };

        std::wstring str(len, L'\0');
        in.read(reinterpret_cast<char*>(str.data()), len * sizeof(wchar_t));
        return vData{ str };
    }
    case vDataType::NIL:
    default:
        return vData{ std::monostate{} };
    }
}

#endif