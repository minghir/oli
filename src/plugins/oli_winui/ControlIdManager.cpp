#include "ControlIdManager.hpp"
// Nu este necesar ConsoleManager.hpp aici, deoarece nu se face logare directă.
// Logarea ar trebui să se facă în clasele care utilizează ControlIdManager.

// --- Implementarea metodei allocate ---
int ControlIdManager::allocate(const std::string& id) {
    // Blochează mutex-ul pentru a asigura acces exclusiv la mape.
    // std::lock_guard asigură eliberarea automată a blocării la ieșirea din scope.
    std::lock_guard<std::mutex> lock(mtx);

    // Verifică dacă ID-ul string a fost deja alocat.
    auto it = idMap.find(id);
    if (it != idMap.end()) {
        // Dacă ID-ul există deja, returnează ID-ul numeric asociat.
        return it->second;
    }

    // Alocă următorul ID numeric disponibil și incrementează contorul.
    int newId = nextId++;
    // Asociază ID-ul string cu noul ID numeric.
    idMap[id] = newId;
    // Asociază noul ID numeric cu ID-ul string (pentru căutare inversă).
    reverseMap[newId] = id;

    return newId;
}

// --- Implementarea metodei getNameById ---
std::string ControlIdManager::getNameById(int win32Id) {
    // Blochează mutex-ul pentru a asigura acces exclusiv la mape.
    std::lock_guard<std::mutex> lock(mtx);

    // Caută ID-ul numeric în mapa inversă.
    auto it = reverseMap.find(win32Id);
    // Dacă ID-ul este găsit, returnează ID-ul string asociat; altfel, returnează "<unknown>".
    return (it != reverseMap.end()) ? it->second : "<unknown>";
}