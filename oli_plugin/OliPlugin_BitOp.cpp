#include "../src/OliEngine.hpp" 
#include <vector>
#include <map>
#include <functional>
#include <bitset>
#include <variant>
#include <bit> // Pentru std::popcount în C++20


// Helper: Conversie sigură vData -> long long pentru operații pe biți
inline long long toInt(const vData& v) {
    if (std::holds_alternative<long long>(v.value))
        return std::get<long long>(v.value);

    if (std::holds_alternative<double>(v.value))
        return static_cast<long long>(std::get<double>(v.value));

    return 0;
}

void RegisterBitOpFunctions(std::unordered_map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry) {

    // --- Operații Bitwise Binare ---

    // BIT_AND(a, b) -> a & b
    registry[L"BIT_AND"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0LL };
        return vData{ toInt(a[0]) & toInt(a[1]) };
    };

    // BIT_OR(a, b) -> a | b
    registry[L"BIT_OR"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0LL };
        return vData{ toInt(a[0]) | toInt(a[1]) };
    };

    // BIT_XOR(a, b) -> a ^ b
    registry[L"BIT_XOR"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0LL };
        return vData{ toInt(a[0]) ^ toInt(a[1]) };
    };

    // --- Operații de deplasare (Shift) ---

    // BIT_LSHIFT(val, count) -> val << count
    registry[L"BIT_LSHIFT"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0LL };
        return vData{ toInt(a[0]) << toInt(a[1]) };
    };

    // BIT_RSHIFT(val, count) -> val >> count
    registry[L"BIT_RSHIFT"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0LL };
        return vData{ toInt(a[0]) >> toInt(a[1]) };
    };

    // --- Operații Unare ---

    // BIT_NOT(a) -> ~a
    registry[L"BIT_NOT"] = [=](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0LL };
        return vData{ ~toInt(a[0]) };
    };

    // --- Utilitare pentru Validare ---

    // BIT_CHECK(val, bit_pos) -> returnează 1 dacă bitul de pe poziția n este setat, altfel 0
    registry[L"BIT_CHECK"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0LL };
        long long val = toInt(a[0]);
        long long pos = toInt(a[1]);
        return vData{ (val & (1LL << pos)) ? 1LL : 0LL };
    };

    // BIT_SET(val, bit_pos) -> returnează valoarea cu bitul respectiv activat
    registry[L"BIT_SET"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0LL };
        return vData{ toInt(a[0]) | (1LL << toInt(a[1])) };
    };

    // BIT_COUNT(val) -> Numără câți biți de 1 sunt în număr (Popcount)
    registry[L"BIT_COUNT"] = [=](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0LL };
        unsigned long long val = static_cast<unsigned long long>(toInt(a[0]));

        // Dacă nu ai C++20, poți folosi __popcountll(val) pe GCC/Clang 
        // sau o metodă manuală pentru portabilitate:
        int count = 0;
        while (val) {
            val &= (val - 1);
            count++;
        }
        return vData{ static_cast<long long>(count) };
    };

    // BIT_TO_BIN(val) -> Returnează un string cu reprezentarea binară (ex: "1010")
    registry[L"BIT_TO_BIN"] = [=](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ L"0" };
        long long val = toInt(a[0]);

        if (val == 0) return vData{ L"0" };

        // Convertim în string binar folosind bitset (pe 64 de biți)
        std::string binaryStr = std::bitset<64>(val).to_string();

        // Eliminăm zerourile nesemnificative din stânga pentru a fi lizibil
        size_t firstOne = binaryStr.find('1');
        if (firstOne == std::string::npos) return vData{ L"0" };

        std::string trimmed = binaryStr.substr(firstOne);

        // Convertim din std::string în std::wstring pentru vData
        std::wstring wResult(trimmed.begin(), trimmed.end());
        return vData{ wResult };
    };

    // CHECK_MASK(val, mask) -> returnează 1 dacă TOȚI biții din 'mask' sunt prezenți în 'val'
    registry[L"CHECK_MASK"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0LL };
        long long val = toInt(a[0]);
        long long mask = toInt(a[1]);

        // Logica: facem AND între valoare și mască. 
        // Rezultatul trebuie să fie identic cu masca pentru a confirma că toți biții sunt acolo.
        return vData{ ((val & mask) == mask) ? 1LL : 0LL };
    };

    // ANY_MASK(val, mask) -> returnează 1 dacă MĂCAR UNUL dintre biții din 'mask' se află în 'val'
    registry[L"ANY_MASK"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0LL };
        long long val = toInt(a[0]);
        long long mask = toInt(a[1]);

        // Logica: Dacă rezultatul operației AND este mai mare decât 0, 
        // înseamnă că există cel puțin un bit de '1' comun.
        return vData{ ((val & mask) > 0) ? 1LL : 0LL };
    };
}
