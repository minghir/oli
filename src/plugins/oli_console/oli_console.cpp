#include "../../OliEngine.hpp"
#include <unordered_map> // Schimbat din <map>
#include <string>
#include <functional>
#include <iostream>


#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#include <windows.h>
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// Folosim alias-ul pentru a păstra codul curat (asigură-te că e identic cu cel din OliEngine.hpp)
using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;






void RegisterConsoleFunctions(std::unordered_map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry) {

    // CON_SCREEN(width, height) - Setează dimensiunea ferestrei (Windows only)
    registry[L"CON_SCREEN"] = [](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ false };
#ifdef _WIN32
        int w = (int)std::get<long long>(a[0].value);
        int h = (int)std::get<long long>(a[1].value);

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        COORD size = { (short)w, (short)h };

        // Setăm fereastra la o dimensiune minimă înainte de a schimba buffer-ul
        // (Windows nu te lasă să micșorezi buffer-ul sub dimensiunea ferestrei curente)
        SMALL_RECT tmpRect = { 0, 0, 1, 1 };
        SetConsoleWindowInfo(hOut, TRUE, &tmpRect);

        // Setăm dimensiunea buffer-ului (memoria internă)
        SetConsoleScreenBufferSize(hOut, size);

        // Setăm dimensiunea ferestrei vizibile la aceeași valoare
        SMALL_RECT finalRect = { 0, 0, (short)(w - 1), (short)(h - 1) };
        SetConsoleWindowInfo(hOut, TRUE, &finalRect);
#endif
        return vData{ false };
        };

    // PUT_AT(x, y, string) - Mută cursorul și scrie ceva
    registry[L"PUT_AT"] = [](const std::vector<vData>& a) -> vData {
        if (a.size() < 3) return vData{ false };
        int x = (int)std::get<long long>(a[0].value);
        int y = (int)std::get<long long>(a[1].value);
        std::wstring text = std::get<std::wstring>(a[2].value);

#ifdef _WIN32
        COORD coord = { (short)x, (short)y };
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
        std::wcout << text;
#else
        // ANSI Escape: \033[y;xH (atenție, ANSI e 1-based, Oli e probabil 0-based)
        std::wcout << L"\033[" << (y + 1) << L";" << (x + 1) << L"H" << text;
#endif
        return vData{ true };
        };

    // CLS() - Șterge ecranul
    registry[L"CON_CLS"] = [](const std::vector<vData>&) -> vData {
#ifdef _WIN32
        system("cls");
#else
        std::wcout << L"\033[2J\033[H";
#endif
        return vData{ true };
        };

    // CURSOR(visible) - Ascunde/arată cursorul (esențial pentru jocuri să nu pâlpâie)
    registry[L"CURSOR"] = [](const std::vector<vData>& a) -> vData {
        bool show = true;
        if (!a.empty()) {
            // Dacă e 0, false sau null, ascundem
            if (std::holds_alternative<bool>(a[0].value)) show = std::get<bool>(a[0].value);
            else if (std::holds_alternative<long long>(a[0].value)) show = std::get<long long>(a[0].value) != 0;
        }

#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO ci;
        GetConsoleCursorInfo(hOut, &ci);
        ci.bVisible = show;
        SetConsoleCursorInfo(hOut, &ci);
        return vData{ true };
#endif
        return vData{ false };
        };
}


OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {
    RegisterConsoleFunctions(registry);
}