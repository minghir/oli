#include "../../IOliEngine.hpp"
#include "../../vData.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <functional>

#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// Satisfacem verificarea de consolă a motorului pentru ambele moduri
OLI_EXPORT void SetPluginConsoleManager(void* dummy) {}

// 1. Comanda: STATS
void HandleStats(const std::wstring& line, IOliEngine* engine) {
    engine->logSuccess(L"--- OliPower Plugin Stats ---");
    engine->logSuccess(L"Status: Operational (Bytecode Ready)");
    engine->logSuccess(L"API Version: 1.1");
    engine->logSuccess(L"-----------------------------");
}

// 2. Comanda: INJECT_HERO
void HandleInjectHero(const std::wstring& line, IOliEngine* engine) {
    vData heroName(L"Oli-Warrior");

    // Folosim '@' pentru a-i spune VM-ului în assignToByteCodeVariable 
    // să o scrie direct în tabela globală, ocolind stiva locală din .olic
    engine->setVar(L"hero_name", heroName);

    engine->logSuccess(L"Injected $hero_name into Oli memory!");
}

// 3. Punctul de intrare universal (Entry Point)
OLI_EXPORT void LoadOliCommandPlugin(
    std::unordered_map<std::wstring, std::function<void(const std::wstring&)>>& handlers,
    IOliEngine* engine) {

    handlers[L"STATS"] = [engine](const std::wstring& line) {
        HandleStats(line, engine);
        };

    handlers[L"INJECT_HERO"] = [engine](const std::wstring& line) {
        HandleInjectHero(line, engine);
        };
}