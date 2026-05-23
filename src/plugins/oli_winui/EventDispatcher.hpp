
#ifndef EVENT_DISPATCHER_HPP
#define EVENT_DISPATCHER_HPP

#pragma once

#include <functional>   // Pentru std::function (pentru callback-uri)
#include <string>       // Pentru std::string (ID-uri, nume evenimente)
#include <unordered_map>// Pentru std::unordered_map (colecții de handleri)
#include <vector>       // Pentru std::vector (pentru multiple handleri generici)
#include <windows.h>    // Pentru tipuri WinAPI (UINT)

// Clasa EventDispatcher gestionează înregistrarea și declanșarea evenimentelor.
// Oferă două mecanisme de dispecerizare:
// 1. Evenimente specifice WinAPI (legate de un ID de control și un mesaj WinAPI).
// 2. Evenimente generice (identificate printr-un nume de șir de caractere, ex: "click").
class EventDispatcher {
public:
    // Alias pentru tipul de funcție callback folosit pentru evenimente.
    // Callback-urile sunt de tip `void()` (nu primesc parametri și nu returnează nimic).
    using EventCallback = std::function<void()>;

    // --- Metode pentru evenimente legate de ID-uri de control și mesaje WinAPI specifice ---


     // NOU: Alias pentru callback-uri care acceptă un singur argument std::string
    using EventCallbackWithArg = std::function<void(const std::string&)>; // ⬅️ NOU
    /**
     * @brief Înregistrează un handler care acceptă un argument std::string, asociat cu un ID de control.
     * @param eventName Numele evenimentului (ex: "grid_column_click").
     * @param controlId ID-ul controlului care declanșează evenimentul (ex: "mainGrid").
     * @param callback Funcția ce va fi apelată (acceptă un std::string).
     */
    void registerHandler(const std::string& eventName, const std::string& controlId, EventCallbackWithArg callback); // ⬅️ NOU

     /**
     * @brief Declanșează un eveniment, trecând un argument std::string.
     * @param eventName Numele evenimentului.
     * @param controlId ID-ul controlului sursă.
     * @param argument Argumentul de tip std::string care va fi transmis handlerului.
     * @return true dacă un handler a fost găsit și executat, false altfel.
     */
    bool dispatch(const std::string& eventName, const std::string& controlId, const std::string& argument); // ⬅️ NOU


    /**
     * @brief Înregistrează un handler (funcție callback) pentru un eveniment specific.
     * Acest handler este asociat cu un anumit ID de control și un mesaj WinAPI.
     * Un singur handler poate fi înregistrat per pereche (id, message);
     * înregistrarea ulterioară va suprascrie handlerul existent.
     * @param id ID-ul unic al controlului (intern, de obicei std::string).
     * @param message Mesajul WinAPI (ex: WM_COMMAND, WM_NOTIFY).
     * @param callback Funcția ce va fi apelată când evenimentul este declanșat.
     */
    void registerHandler(const std::string& id, UINT message, EventCallback callback);

    /**
     * @brief Declanșează un eveniment specific, executând handlerul asociat cu
     * perechea (ID control, mesaj WinAPI).
     * @param id ID-ul controlului.
     * @param message Mesajul WinAPI.
     * @return true dacă un handler a fost găsit și executat, false altfel.
     */
    bool dispatch(const std::string& id, UINT message);

    /**
     * @brief O metodă convenabilă pentru a declanșa evenimente WM_COMMAND pentru un anumit ID.
     * Aceasta este adesea folosită pentru evenimentele de tip "click" ale butoanelor.
     * @param id ID-ul controlului.
     * @return Rezultatul apelului dispatch cu WM_COMMAND.
     */
    bool emit(const std::string& id);


    // 2. Înregistrare pentru evenimente generice, specifice unui control (Nume Eveniment + ID Control)
    void registerHandler(const std::string& eventName, const std::string& controlId, EventCallback callback);
    bool dispatch(const std::string& eventName, const std::string& controlId = "");



    // --- Metode pentru evenimente generice, bazate doar pe numele (șirul de caractere) ---

    /**
     * @brief Înregistrează un handler pentru un eveniment generic (ex: "click", "hover", "onClose").
     * Permite înregistrarea mai multor handleri pentru același nume de eveniment,
     * toți fiind executați la declanșare.
     * @param eventName Numele evenimentului (șir de caractere).
     * @param callback Funcția ce va fi apelată când evenimentul este declanșat.
     */
    void registerHandler(const std::string& eventName, EventCallback callback);

    /**
     * @brief Declanșează un eveniment generic, bazat doar pe numele său.
     * Va apela toți handlerii înregistrați pentru acel nume de eveniment.
     * @param eventName Numele evenimentului (șir de caractere).
     * @return true dacă unul sau mai mulți handleri au fost găsiți și executați, false altfel.
     */
   // bool dispatch(const std::string& eventName);
    bool renameControlHandlers(const std::string& oldId, const std::string& newId);

    bool removeHandlers(const std::string& controlId);
    bool removeHandler(const std::string& eventName, const std::string& controlId);

private:
    // Harta pentru evenimente legate de ID-uri de control și mesaje WinAPI specifice.
    // Structura: `ID_Control` -> `Mesaj_WinAPI` -> `EventCallback`.
    // Permite un singur handler per pereche (ID, Mesaj).
    std::unordered_map<std::string, std::unordered_map<UINT, EventCallback>> m_handlers;

    // Nouă hartă pentru evenimentele generice bazate pe ID-ul controlului
    std::unordered_map<std::string, std::unordered_map<std::string, EventCallback>> m_controlSpecificHandlers;


    // Harta pentru evenimente generice, bazate doar pe numele (șirul de caractere).
    // Structura: `Nume_Eveniment` -> `Vector_de_Callback-uri`.
    // Permite multiple handleri pentru același nume de eveniment.
   // std::unordered_map<std::string, std::vector<EventCallback>> m_genericHandlers;
   // 
    // Harta pentru evenimentele generice globale
    std::unordered_map<std::string, std::vector<EventCallback>> m_globalHandlers;



    // NOU: Hartă pentru evenimente generice, specifice unui control, care au un argument
    // Structura: `eventName` -> `controlId` -> `EventCallbackWithArg`
    std::unordered_map<std::string, std::unordered_map<std::string, EventCallbackWithArg>> m_controlSpecificHandlersWithArg; // ⬅️ NOU


};

#endif // EVENT_DISPATCHER_HPP