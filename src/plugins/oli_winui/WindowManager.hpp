#ifndef WINDOW_MANAGER_HPP
#define WINDOW_MANAGER_HPP

#pragma once

#include "vWindow.hpp" // Managerul gestionează obiecte vWindow
#include <memory>      // Pentru std::unique_ptr
#include <map>         // Pentru std::map<std::string, std::unique_ptr<vWindow>>
#include <string>      // Pentru std::string (ID-urile ferestrelor)

// Clasa WindowManager este responsabilă pentru gestionarea colecției de ferestre vWindow.
// Acționează ca un container pentru `std::unique_ptr` de vWindow,
// asigurând că ferestrele sunt adăugate, recuperate și distruse corect.
class WindowManager {
public:
    // Adaugă o fereastră la manager.
    // Preia proprietatea asupra `std::unique_ptr<vWindow>` furnizat.
    // id: ID-ul unic al ferestrei (string).
    // win: Un `std::unique_ptr` către obiectul vWindow de adăugat.
    void add(const std::string& id, std::unique_ptr<vWindow> win);

    // Returnează un pointer (care nu deține proprietatea) către o fereastră după ID-ul său.
    // Returnează `nullptr` dacă fereastra nu este găsită.
    // id: ID-ul ferestrei de căutat.
    vWindow* get(const std::string& id);

    // Elimină o fereastră din manager pe baza ID-ului său.
    // Aceasta va distruge automat obiectul vWindow asociat, eliberând resursele.
    // id: ID-ul ferestrei de eliminat.
    void remove(const std::string& id);

    // Oprește managerul de ferestre.
    // Aceasta va goli colecția de ferestre, declanșând distrugerea
    // tuturor obiectelor vWindow deținute.
    void shutdown();

    /**
    * @brief Returnează un pointer (care nu deține proprietatea) către o fereastră
    * pe baza HWND-ului său.
    * @param hwnd HWND-ul ferestrei de căutat.
    * @return Un pointer la obiectul vWindow sau nullptr dacă nu este găsită.
    */
    vWindow* getWindowByHandle(HWND hwnd);

    vWindow* getFirstWindow() {

        if (m_windows.empty()) return nullptr;
        return m_windows.begin()->second.get(); // Returnează prima fereastră adăugată
    }

    bool exists(const std::string& id) const {
        return m_windows.find(id) != m_windows.end();
    }

private:
    // O mapă care stochează ferestrele, utilizând ID-ul lor ca cheie.
    // `std::unique_ptr` asigură gestionarea automată a memoriei și a ciclului de viață.
    std::map<std::string, std::unique_ptr<vWindow>> m_windows;
};

#endif // WINDOW_MANAGER_HPP