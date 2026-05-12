#include "../../IOliEngine.hpp"
#include "../../vData.hpp" // Necesar pentru definirea vData
#include <iostream>
#include <string>

// 1. Comanda: STATS
// Primește 'line' ca string brut pentru a nu strica interpretorul
void HandleStats(const std::wstring& line, IOliEngine* engine) {
    engine->logSuccess(L"--- OliPower Plugin Stats ---");
    engine->logSuccess(L"Status: Operational (Bytecode Ready)");
    engine->logSuccess(L"API Version: 1.1");
    engine->logSuccess(L"-----------------------------");
}

// 2. Comanda: INJECT_HERO
void HandleInjectHero(const std::wstring& line, IOliEngine* engine) {
    vData heroName(L"Oli-Warrior");

    // NOTĂ: setVar va detecta automat dacă suntem într-o funcție de bytecode
    // și va scrie valoarea pe stivă (local) sau în tabela globală.
    engine->setVar(L"hero_name", heroName);
    engine->logSuccess(L"Injected $hero_name into Oli memory!");
}

// 3. Punctul de intrare (Entry Point)
// Semnătura rămâne pe std::wstring pentru compatibilitate cu m_commandHandlers din motor
extern "C" __declspec(dllexport) void LoadOliCommandPlugin(
    std::unordered_map<std::wstring, std::function<void(const std::wstring&)>>& handlers,
    IOliEngine* engine
) {
    // Înregistrăm comenzile în map-ul primit de la motor
    handlers[L"STATS"] = [engine](const std::wstring& line) {
        HandleStats(line, engine);
    };

    handlers[L"INJECT_HERO"] = [engine](const std::wstring& line) {
        HandleInjectHero(line, engine);
    };
}

/*
#include "../../IOliEngine.hpp"

#include <iostream>

// Comanda: STATS
// Afișează un mesaj de succes folosind interfața engine-ului
void HandleStats(const std::wstring& line, IOliEngine* engine) {
    engine->logSuccess(L"--- OliPower Plugin Stats ---");
    engine->logSuccess(L"Status: Operational");
    engine->logSuccess(L"API Version: 1.0");
    engine->logSuccess(L"-----------------------------");
}

// Comanda: INJECT_HERO
// Creează automat o variabilă în motorul Oli
void HandleInjectHero(const std::wstring& line, IOliEngine* engine) {
    // Putem crea un mic obiect direct
    vData heroName(L"Oli-Warrior");

    engine->setVar(L"hero_name", heroName);
    engine->logSuccess(L"Injected $hero_name into Oli memory!");
}

// Punctul de intrare cerut de vOliEngine::handlePluginCommand
extern "C" __declspec(dllexport) void LoadOliCommandPlugin(
    std::unordered_map<std::wstring, std::function<void(const std::wstring&)>>& handlers,
    IOliEngine* engine
) {
    // Înregistrăm comenzile
    // Lambda-ul capturează pointerul 'engine' pentru a-l transmite handlerelor
    handlers[L"STATS"] = [engine](const std::wstring& line) {
        HandleStats(line, engine);
        };

    handlers[L"INJECT_HERO"] = [engine](const std::wstring& line) {
        HandleInjectHero(line, engine);
        };
}
*/