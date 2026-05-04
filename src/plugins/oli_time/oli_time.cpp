#include "../../OliEngine.hpp" 

#ifdef _WIN32

#define OLI_EXPORT extern "C" __declspec(dllexport)
#else
#define OLI_EXPORT extern "C"
#endif

#include <chrono>
#include <string>

#include <thread>

// Adaugă asta aici:
inline double toDouble(const vData& v) {
    if (std::holds_alternative<double>(v.value))
        return std::get<double>(v.value);
    if (std::holds_alternative<long long>(v.value))
        return static_cast<double>(std::get<long long>(v.value));
    return 0.0;
}

void RegisterTimeFunctions(std::unordered_map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry) {

    // Returnează timpul actual în milisecunde (Unix Epoch)
    // Util pentru: set t1 = TICKS()
    registry[L"TICKS"] = [=](const std::vector<vData>&) -> vData {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        return vData{ static_cast<long long>(ms) };
        };

    // Returnează timpul în secunde (cu zecimale pentru precizie)
    registry[L"TIME"] = [=](const std::vector<vData>&) -> vData {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        double seconds = std::chrono::duration<double>(now).count();
        return vData{ seconds };
        };

    // O funcție de SLEEP (pauză) - mereu utilă în scripturi
    // Utilizare: SLEEP(1000) -> stă 1 secundă
    registry[L"SLEEP"] = [=](const std::vector<vData>& a) -> vData {
        if (!a.empty()) {
            int ms = static_cast<int>(toDouble(a[0]));
            if (ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            }
        }
        return vData{ std::monostate{} };
        };
}