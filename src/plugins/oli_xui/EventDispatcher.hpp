#ifndef EVENT_DISPATCHER_HPP
#define EVENT_DISPATCHER_HPP

#pragma once
#include <functional>   // Pentru std::function (pentru callback-uri)
#include <string>       // Pentru std::string (ID-uri, nume evenimente)
#include <unordered_map>// Pentru std::unordered_map (colecții de handleri)
#include <vector>       // Pentru std::vector (pentru multiple handleri generici)

// Clasa EventDispatcher gestionează înregistrarea și declanșarea evenimentelor.
// Oferă mecanisme complet cross-platform (adaptate nativ pentru GTK)
class EventDispatcher {
public:
    using EventCallback = std::function<void()>;
    using EventCallbackWithArg = std::function<void(const std::string&)>;

    // --- 1. Metode pentru evenimente cu argument (ex: selectarea unui tab sau rând în grid) ---
    void registerHandler(const std::string& eventName, const std::string& controlId, EventCallbackWithArg callback);
    bool dispatch(const std::string& eventName, const std::string& controlId, const std::string& argument);

    // --- 2. Păstrarea semnăturilor WinAPI (Mască pentru compatibilitatea cu interpretorul tău) ---
    // Intern, transformăm mesajul numeric într-un string (ex: 273 devine "command") pentru compatibilitate 100%
    void registerHandler(const std::string& id, unsigned int message, EventCallback callback);
    bool dispatch(const std::string& id, unsigned int message);
    bool emit(const std::string& id);

    // --- 3. Metode pentru evenimente generice, specifice unui control (Nume Eveniment + ID Control) ---
    void registerHandler(const std::string& eventName, const std::string& controlId, EventCallback callback);
    bool dispatch(const std::string& eventName, const std::string& controlId = "");

    // --- 4. Metode pentru evenimente generice globale ---
    void registerHandler(const std::string& eventName, EventCallback callback);

    // --- 5. Utilitare pentru managementul dinamic al interfeței din script ---
    bool renameControlHandlers(const std::string& oldId, const std::string& newId);
    bool removeHandlers(const std::string& controlId);
    bool removeHandler(const std::string& eventName, const std::string& controlId);

private:
    // Harta mascată WinAPI: ID_Control -> Mesaj_Numeric -> EventCallback
    std::unordered_map<std::string, std::unordered_map<unsigned int, EventCallback>> m_handlers;

    // Harta evenimentelor specifice unui control: eventName -> controlId -> EventCallback
    std::unordered_map<std::string, std::unordered_map<std::string, EventCallback>> m_controlSpecificHandlers;

    // Harta evenimentelor generice globale
    std::unordered_map<std::string, std::vector<EventCallback>> m_globalHandlers;

    // Harta evenimentelor cu argumente string: eventName -> controlId -> EventCallbackWithArg
    std::unordered_map<std::string, std::unordered_map<std::string, EventCallbackWithArg>> m_controlSpecificHandlersWithArg;
};

#endif // EVENT_DISPATCHER_HPP