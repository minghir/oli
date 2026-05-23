#ifndef CONTROL_ID_MANAGER_HPP
#define CONTROL_ID_MANAGER_HPP

#pragma once

#include <mutex>          // Pentru std::mutex și std::lock_guard (thread-safety)
#include <unordered_map>  // Pentru std::unordered_map (mapare eficientă ID-uri)
#include <string>         // Pentru std::string (numele controlului)

// Clasa ControlIdManager este responsabilă pentru alocarea și gestionarea
// ID-urilor numerice unice (Win32 IDs) pentru controalele UI.
// Este o clasă de tip "singleton static", toate metodele și membrii fiind statici.
// Asigură thread-safety pentru operațiunile sale.
class ControlIdManager {
public:
    /**
     * @brief Alocă un ID numeric Win32 unic pentru un control dat de ID-ul său logic (string).
     * Dacă ID-ul string a fost deja alocat, returnează ID-ul numeric existent.
     * Alocarea este thread-safe.
     * @param id ID-ul logic (string) al controlului (ex: "mainButtonOk").
     * @return ID-ul numeric Win32 alocat sau existent pentru control.
     */
    static int allocate(const std::string& id);

    /**
     * @brief Returnează ID-ul logic (string) al unui control pe baza ID-ului său numeric Win32.
     * Căutarea este thread-safe.
     * @param win32Id ID-ul numeric Win32 al controlului.
     * @return ID-ul logic (string) al controlului sau "<unknown>" dacă ID-ul nu este găsit.
     */
    static std::string getNameById(int win32Id);

private:
    // Un mutex static pentru a asigura thread-safety la accesarea mapelor.
    // 'static inline' permite definirea și inițializarea în header (C++17+).
    static inline std::mutex mtx;

    // Următorul ID numeric disponibil care va fi alocat.
    // Începe de la 1000 pentru a evita coliziunile cu ID-uri de sistem sau predefinite.
    static inline int nextId = 1000;

    // O mapă care asociază ID-ul logic (string) al controlului cu ID-ul său numeric Win32.
    static inline std::unordered_map<std::string, int> idMap;

    // O mapă inversă care asociază ID-ul numeric Win32 cu ID-ul logic (string) al controlului.
    static inline std::unordered_map<int, std::string> reverseMap;

    // Constructorul privat și operatorul de asignare privați
    // previn instanțierea accidentală a clasei (este o clasă statică).
    ControlIdManager() = delete;
    ControlIdManager(const ControlIdManager&) = delete;
    ControlIdManager& operator=(const ControlIdManager&) = delete;
};

#endif // CONTROL_ID_MANAGER_HPP