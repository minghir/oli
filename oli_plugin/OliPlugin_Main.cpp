#include "../src/OliEngine.hpp"
#include <map>
#include <string>
#include <functional>

// Declarăm funcțiile din celelalte fișiere
void RegisterMathFunctions(std::map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry);
//void RegisterDBFunctions(std::map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry);
void RegisterBitOpFunctions(std::map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry);
void RegisterFileSystemFunctions(std::map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry);
void RegisterTimeFunctions(std::map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry);

extern "C" {
    __declspec(dllexport) void LoadOliPlugin(std::map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry) {
        RegisterMathFunctions(registry);
        //RegisterDBFunctions(registry);
        RegisterTimeFunctions(registry);
        RegisterBitOpFunctions(registry);
        RegisterFileSystemFunctions(registry);
    }
}