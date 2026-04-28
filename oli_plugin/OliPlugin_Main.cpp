#include "../src/OliEngine.hpp"
#include <map>
#include <string>
#include <functional>

#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// Declarăm funcțiile din celelalte fișiere
void RegisterMathFunctions(std::map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry);
//void RegisterDBFunctions(std::map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry);
void RegisterBitOpFunctions(std::map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry);
void RegisterFileSystemFunctions(std::map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry);
void RegisterTimeFunctions(std::map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry);
void RegisterKeyboardFunctions(std::map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry);

OLI_EXPORT  void LoadOliPlugin(std::map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry) {
        RegisterMathFunctions(registry);
        //RegisterDBFunctions(registry);
        RegisterTimeFunctions(registry);
        RegisterBitOpFunctions(registry);
        RegisterFileSystemFunctions(registry);
		RegisterKeyboardFunctions(registry);
    }
