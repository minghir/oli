#include "../../OliEngine.hpp" 

#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#include <windows.h>
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#include <unistd.h>
#endif

#include <vector>
#include <map>
#include <functional>
#include <cmath>
#include <string>
#include <algorithm>

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

// Helper: Conversie sigură vData -> double
inline double toDouble(const vData& v) {
    if (std::holds_alternative<double>(v.value))
        return std::get<double>(v.value);

    if (std::holds_alternative<long long>(v.value))
        return static_cast<double>(std::get<long long>(v.value));

    return 0.0;
}

void RegisterMathFunctions(std::unordered_map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry) {
    // Definim constantele local. Folosind [=] în lambda, fiecare funcție 
    // va primi propria COPIE a acestor valori, evitând coruperea memoriei.
    const double PI_VAL = 3.14159265358979323846;
    const double E_VAL = 2.71828182845904523536;

    // --- Operații aritmetice ---
    registry[L"ADD"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0.0 };
        return vData{ toDouble(a[0]) + toDouble(a[1]) };
        };

    registry[L"SUB"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0.0 };
        return vData{ toDouble(a[0]) - toDouble(a[1]) };
        };

    registry[L"MUL"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0.0 };
        return vData{ toDouble(a[0]) * toDouble(a[1]) };
        };

    registry[L"DIV"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0.0 };
        double b = toDouble(a[1]);
        return vData{ (b == 0) ? 0.0 : toDouble(a[0]) / b };
        };

    registry[L"MOD"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0.0 };
        long long x = static_cast<long long>(toDouble(a[0]));
        long long y = static_cast<long long>(toDouble(a[1]));
        return vData{ static_cast<double>((y == 0) ? 0 : x % y) };
        };

    // --- Funcții matematice ---
    registry[L"SQRT"] = [=](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0.0 };
        return vData{ std::sqrt(toDouble(a[0])) };
        };

    registry[L"POW"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0.0 };
        return vData{ std::pow(toDouble(a[0]), toDouble(a[1])) };
        };

    registry[L"ABS"] = [=](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0.0 };
        return vData{ std::fabs(toDouble(a[0])) };
        };

    registry[L"MIN"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0.0 };
        return vData{ std::min<double>(toDouble(a[0]), toDouble(a[1])) };
        };

    registry[L"MAX"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0.0 };
        return vData{ std::max<double>(toDouble(a[0]), toDouble(a[1])) };
        };

    registry[L"ROUND"] = [=](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0.0 };
        return vData{ std::round(toDouble(a[0])) };
        };

    // --- Trigonometrie (Input în grade) ---
    registry[L"SIN"] = [=](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0.0 };
        double rad = toDouble(a[0]) * (PI_VAL / 180.0);
        return vData{ std::sin(rad) };
        };

    registry[L"SINR"] = [=](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0.0 };
        return vData{ std::sin(toDouble(a[0])) };
        };

    registry[L"COS"] = [=](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0.0 };
        double rad = toDouble(a[0]) * (PI_VAL / 180.0);
        return vData{ std::cos(rad) };
        };

    registry[L"COSR"] = [=](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0.0 };
        return vData{ std::cos(toDouble(a[0])) };
        };

    registry[L"TAN"] = [=](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0.0 };
        double rad = toDouble(a[0]) * (PI_VAL / 180.0);
        return vData{ std::tan(rad) };
        };

    // --- Constante ---
    registry[L"PI"] = [=](const std::vector<vData>&) -> vData {
        return vData{ PI_VAL };
        };

    registry[L"E"] = [=](const std::vector<vData>&) -> vData {
        return vData{ E_VAL };
        };
    

    registry[L"FLOOR"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(0.0);
        return vData(std::floor(args[0].toDouble()));
        };

    //lerp(a, b, t) = a + t \times(b - a)$$
    /*
    registry[L"LERP"] = [=](const std::vector<vData>& args) -> vData {
        // Avem nevoie de 3 argumente: start, end, t
        if (args.size() < 3) return vData{ 0.0 };

        double a = args[0].toDouble();
        double b = args[1].toDouble();
        double t = args[2].toDouble();

        // Clamp opțional pentru t între 0 și 1 pentru siguranță
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;

        return vData{ a + t * (b - a) };
        };
        */
    registry[L"LERP"] = [=](const std::vector<vData>& a) -> vData {
        if (a.size() < 3) return vData{ 0.0 };
        // std::lerp(start, end, t) - disponibil în C++20
        return vData{ std::lerp(toDouble(a[0]), toDouble(a[1]), toDouble(a[2])) };
        };

    // --- Utilități de limitare (C++20) ---
    registry[L"CLAMP"] = [=](const std::vector<vData>& a) -> vData {
        // Avem nevoie de: valoare, minim, maxim
        if (a.size() < 3) return vData{ 0.0 };

        double val = toDouble(a[0]);
        double low = toDouble(a[1]);
        double high = toDouble(a[2]);

        // std::clamp se află în <algorithm>
        return vData{ std::clamp(val, low, high) };
        };
}


OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {
    RegisterMathFunctions(registry);
}