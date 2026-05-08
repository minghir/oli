#ifndef VDATASERIALIZE_HPP
#define VDATASERIALIZE_HPP

#include "vData.hpp"
#include <iostream>
#include <variant>
#include <string>

// --- 1. FORWARD DECLARATIONS ---
// Îi spunem compilatorului că aceste tipuri există undeva
class vOliEngine;
struct OliChunk;
struct ByteCodeProcedure;

namespace vDataSerialize {
    enum class vDataType : uint8_t {
        NIL = 0, INT = 1, DOUBLE = 2, STRING = 3, BOOL = 4, ARRAY = 5, MAP = 6
    };

    inline void serializevData(const vData& data, std::ostream& out);
    inline vData deserializevData(std::istream& in);
    inline void serializeWString(const std::wstring& str, std::ostream& out);
    inline std::wstring deserializeWString(std::istream& in);


    inline void serializeWString(const std::wstring& str, std::ostream& out) {
        uint32_t len = static_cast<uint32_t>(str.size());
        out.write(reinterpret_cast<const char*>(&len), sizeof(len));
        if (len > 0) {
            // Scriem dimensiunea exacta a wchar_t pentru a fi citita corect
            out.write(reinterpret_cast<const char*>(str.data()), len * sizeof(wchar_t));
        }
    }

    inline std::wstring deserializeWString(std::istream& in) {
        uint32_t len = 0;
        in.read(reinterpret_cast<char*>(&len), sizeof(len));
        if (len == 0) return L"";
        std::wstring str(len, L'\0');
        in.read(reinterpret_cast<char*>(str.data()), len * sizeof(wchar_t));
        return str;
    }

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
            out.write(reinterpret_cast<const char*>(&len), sizeof(len));
            if (len > 0) {
                out.write(reinterpret_cast<const char*>(str.data()), len * sizeof(wchar_t));
            }
        }
        // --- LOGICĂ NOUĂ PENTRU ARRAY ---
        else if (actual.isArray()) {
            uint8_t type = (uint8_t)vDataType::ARRAY;
            out.write(reinterpret_cast<const char*>(&type), 1);

            auto* arr = actual.rawArray();
            uint32_t count = arr ? static_cast<uint32_t>(arr->size()) : 0;
            out.write(reinterpret_cast<const char*>(&count), sizeof(count));

            if (arr) {
                for (const auto& item : *arr) {
                    serializevData(item, out); // RECURSIVITATE
                }
            }
        }
        // --- LOGICĂ NOUĂ PENTRU MAP ---
        else if (actual.isMap()) {
            uint8_t type = (uint8_t)vDataType::MAP;
            out.write(reinterpret_cast<const char*>(&type), 1);

            auto* m = actual.rawMap();
            uint32_t count = m ? static_cast<uint32_t>(m->size()) : 0;
            out.write(reinterpret_cast<const char*>(&count), sizeof(count));

            if (m) {
                for (auto const& [key, val] : *m) {
                    // Salvăm cheia ca un string vData
                    serializevData(vData(key), out);
                    // Salvăm valoarea recursiv
                    serializevData(val, out);
                }
            }
        }
    }


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
                              // --- DESERIALIZARE ARRAY ---
        case vDataType::ARRAY: {
            uint32_t count;
            in.read(reinterpret_cast<char*>(&count), sizeof(count));
            vDataArray arr = std::make_shared<std::vector<vData>>();
            for (uint32_t i = 0; i < count; ++i) {
                arr->push_back(deserializevData(in));
            }
            return vData{ arr };
        }
                             // --- DESERIALIZARE MAP ---
        case vDataType::MAP: {
            uint32_t count;
            in.read(reinterpret_cast<char*>(&count), sizeof(count));
            vDataMap m = std::make_shared<std::unordered_map<std::wstring, vData>>();
            for (uint32_t i = 0; i < count; ++i) {
                vData keyData = deserializevData(in);
                vData valData = deserializevData(in);
                if (keyData.isString()) {
                    (*m)[std::get<std::wstring>(keyData.value)] = valData;
                }
            }
            return vData{ m };
        }
        case vDataType::NIL:
        default:
            return vData{ std::monostate{} };
        }
    }


    // --- 4. SERIALIZARE CHUNK / ENGINE (LOGICA COMPLEXĂ) ---

    // Acum compilatorul știe ce e OliChunk și serializevData
    inline void serializeChunk(const OliChunk& chunk, std::ostream& out);

    // Dacă implementarea folosește metode din OliChunk, 
    // e mai bine să o pui după ce incluzi OliBytecode.hpp sau să o lași inline
    // dar compilatorul are nevoie să vadă definiția struct-ului OliChunk pentru a accesa .constants sau .code

    
}//end namespace vDataSerialize

// ATENȚIE: Pentru ca serializeChunk să funcționeze (să acceseze .code, .constants), 
// trebuie să incluzi header-ul unde este definit OliChunk.
#include "olic/OliBytecode.hpp"

namespace vDataSerialize {

    // Definiția corectată (am scos vDataSerialize:: din nume pentru că suntem deja în namespace)
    inline void serializeChunk(const OliChunk& chunk, std::ostream& out) {

        // 1. Scriem Constantele
        uint32_t constCount = (uint32_t)chunk.constants.size();
        out.write(reinterpret_cast<const char*>(&constCount), sizeof(constCount));
        for (const auto& c : chunk.constants) {
            // FIX: Ordinea corectă (Data, Stream)
            vDataSerialize::serializevData(c, out);
        }

        // 2. Scriem Codul (Bytecode)
        uint32_t codeSize = (uint32_t)chunk.code.size();
        out.write(reinterpret_cast<const char*>(&codeSize), sizeof(codeSize));
        if (codeSize > 0) {
            out.write(reinterpret_cast<const char*>(chunk.code.data()), codeSize);
        }

        // 3. Scriem Procedurile
        uint32_t procCount = (uint32_t)chunk.procedures.size();
        out.write(reinterpret_cast<const char*>(&procCount), sizeof(procCount));

        for (auto const& [name, proc] : chunk.procedures) {
            vDataSerialize::serializeWString(proc.name, out);

            uint32_t paramCount = (uint32_t)proc.params.size();
            out.write(reinterpret_cast<const char*>(&paramCount), sizeof(paramCount));
            for (const auto& p : proc.params) {
                vDataSerialize::serializeWString(p, out);
            }

            uint8_t variadic = proc.isVariadic ? 1 : 0;
            out.write(reinterpret_cast<const char*>(&variadic), 1);

            // RECURSIVITATE: FIX ordinea argumentelor (Chunk, Stream)
            vDataSerialize::serializeChunk(*proc.compiledBody, out);
        }
    }

    // Declarația pentru deserializare (asigură-te că implementarea din .cpp respectă ordinea asta)
    void deserializeChunkToEngine(std::istream& in, OliChunk& outChunk, vOliEngine* engine);

    inline std::wstring stringify(const vData& data) {
        if (data.isString()) return std::get<std::wstring>(data.value);
        return data.toWString();
    }
} // end namespace vDataSerialize

#endif



