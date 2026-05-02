#include "../src/OliEngine.hpp"
#include <unordered_map>
#include <functional>
#include <vector>
#include <chrono>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <map>
#endif

#ifndef _WIN32
// 1. DECLARAȚIILE GLOBALE (Trebuie să fie primele)
static std::map<int, std::chrono::steady_clock::time_point> g_linuxKeyMap;

// 2. FUNCȚIA DE SYNC (Folosește g_linuxKeyMap declarat mai sus)
void sync_linux_keys_x11() {
    Display* d = XOpenDisplay(NULL);
    if (!d) return;

    char keys_return[32];
    XQueryKeymap(d, keys_return);

    auto check_key = [&](int x11_keysym, int win_vk) {
        KeyCode kc = XKeysymToKeycode(d, x11_keysym);
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

    XCloseDisplay(d);
}
#endif

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
        short state = GetAsyncKeyState(vk);
        return vData{ (state & 0x8000) ? 1LL : 0LL };
#else
        Display* d = XOpenDisplay(NULL);
        if (!d) return vData{ 0LL };

        char keys[32];
        XQueryKeymap(d, keys);

        int keysym = 0;
        switch (vk) {
        case 37: keysym = XK_Left; break;
        case 39: keysym = XK_Right; break;
        case 38: keysym = XK_Up; break;
        case 40: keysym = XK_Down; break;
        case 32: keysym = XK_space; break;
        case 81: keysym = XK_q; break;
        default: keysym = vk; // Încearcă mapare directă pentru caractere
        }

        KeyCode kc = XKeysymToKeycode(d, keysym);
        bool isPressed = (kc != 0) && (keys[kc >> 3] & (1 << (kc & 7)));

        XCloseDisplay(d);
        return vData{ isPressed ? 1LL : 0LL };
#endif
        };

    registry[L"KBD_RESTORE"] = [=](const std::vector<vData>&) -> vData {
        return vData{ true };
        };
}