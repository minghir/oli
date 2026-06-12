#include "../../OliEngine.hpp" 

#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#include <windows.h>
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#include <unistd.h>
#endif

#include <unordered_map>
#include <functional>
#include <vector>
#include <chrono>
#include <algorithm>
#include <iostream>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <map>
#endif

// Structura globală OpenGL partajată (dacă este accesibilă) sau fallback intern
#ifndef _WIN32
static std::map<int, std::chrono::steady_clock::time_point> g_linuxKeyMap;
static Display* g_KbdDisplay = nullptr; // Conexiune persistentă pentru a evita lag-ul

void sync_linux_keys_x11() {
    if (!g_KbdDisplay) g_KbdDisplay = XOpenDisplay(NULL);
    if (!g_KbdDisplay) return;

    char keys_return[32];
    XQueryKeymap(g_KbdDisplay, keys_return);

    auto check_key = [&](int x11_keysym, int win_vk) {
        KeyCode kc = XKeysymToKeycode(g_KbdDisplay, x11_keysym);
        bool pressed = keys_return[kc >> 3] & (1 << (kc & 7));
        if (pressed) {
            g_linuxKeyMap[win_vk] = std::chrono::steady_clock::now();
        }
    };

    check_key(XK_Left, 37);
    check_key(XK_Right, 39);
    check_key(XK_Up, 38);
    check_key(XK_Down, 40);
    check_key(XK_space, 32);
    check_key(XK_q, 81);
    check_key(XK_Escape, 27); // 🔥 Adăugat ESC în sync global
}
#endif

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

long long asInt(const vData& data) {
    if (std::holds_alternative<long long>(data.value)) return std::get<long long>(data.value);
    if (std::holds_alternative<double>(data.value)) return static_cast<long long>(std::get<double>(data.value));
    return 0;
}

void RegisterKeyboardFunctions(std::unordered_map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry) {

    registry[L"GET_KEY"] = [=](const std::vector<vData>&) -> vData {
#ifdef _WIN32
        if (!_kbhit()) return vData{ 0LL };
        return vData{ (long long)_getch() };
#else
        sync_linux_keys_x11();
        if (g_linuxKeyMap.empty()) return vData{ 0LL };
        return vData{ (long long)g_linuxKeyMap.rbegin()->first };
#endif
    };

    registry[L"KEY_STATE"] = [=](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0LL };
        int vk = (int)asInt(a[0]);

#ifdef _WIN32
        return vData{ (GetAsyncKeyState(vk) & 0x8000) ? 1LL : 0LL };
#else
        // Optimizare: Deschidem conexiunea o singură dată, nu la fiecare cadru!
        if (!g_KbdDisplay) g_KbdDisplay = XOpenDisplay(NULL);
        if (!g_KbdDisplay) return vData{ 0LL };

        char keys[32];
        XQueryKeymap(g_KbdDisplay, keys);

        int kc = 0;
        switch (vk) {
            case 27: kc = 9;   break; // 🔥 FIX: Adăugat ESC (Keycode standard Linux = 9)
            case 37: kc = 113; break; // Left
            case 39: kc = 114; break; // Right
            case 38: kc = 111; break; // Up
            case 40: kc = 116; break; // Down
            case 32: kc = 65;  break; // Space
            case 81: kc = 24;  break; // Q
            default: {
                // Fallback dinamic pentru alte coduri nementionate
                KeyCode dynamic_kc = XKeysymToKeycode(g_KbdDisplay, vk == 27 ? XK_Escape : vk);
                kc = (int)dynamic_kc;
                break;
            }
        }

        bool isPressed = false;
        if (kc > 0 && kc < 256) {
            isPressed = (keys[kc >> 3] & (1 << (kc & 7))) != 0;
        }

        return vData{ isPressed ? 1LL : 0LL };
#endif
    };

    registry[L"KBD_RESTORE"] = [=](const std::vector<vData>&) -> vData {
#ifdef _WIN32
        HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
        if (hStdIn != INVALID_HANDLE_VALUE) {
            FlushConsoleInputBuffer(hStdIn);
        }
#else
        g_linuxKeyMap.clear();
        if (g_KbdDisplay) {
            XCloseDisplay(g_KbdDisplay);
            g_KbdDisplay = nullptr;
        }
#endif
        return vData{ 1LL };
    };
}

OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {
    RegisterKeyboardFunctions(registry);
}