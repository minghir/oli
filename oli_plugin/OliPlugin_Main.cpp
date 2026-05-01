#include "../src/OliEngine.hpp"
#include <unordered_map> // Schimbat din <map>
#include <string>
#include <functional>

#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// Folosim alias-ul pentru a păstra codul curat (asigură-te că e identic cu cel din OliEngine.hpp)
using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

// --- DECLARAȚII ACTUALIZATE ---
void RegisterMathFunctions(PluginRegistry& registry);
void RegisterBitOpFunctions(PluginRegistry& registry);
void RegisterFileSystemFunctions(PluginRegistry& registry);
void RegisterTimeFunctions(PluginRegistry& registry);
void RegisterKeyboardFunctions(PluginRegistry& registry);

// --- EXPORT ACTUALIZAT ---
OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {
    RegisterMathFunctions(registry);
    RegisterTimeFunctions(registry);
    RegisterBitOpFunctions(registry);
    RegisterFileSystemFunctions(registry);
    RegisterKeyboardFunctions(registry);
}