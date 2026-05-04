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
        Display* d = XOpenDisplay(NULL);
        if (!d) return vData{ 0LL };

        char keys[32];
        XQueryKeymap(d, keys);

        // KeyCodes fixe pentru Linux (evităm overhead-ul XKeysymToKeycode în buclă)
        int kc = 0;
        switch (vk) {
        case 37: kc = 113; break; // Left
        case 39: kc = 114; break; // Right
        case 38: kc = 111; break; // Up
        case 40: kc = 116; break; // Down
        case 32: kc = 65;  break; // Space
        case 81: kc = 24;  break; // Q
        }

        bool isPressed = false;
        if (kc > 0) {
            isPressed = keys[kc >> 3] & (1 << (kc & 7));
        }

        XCloseDisplay(d);
        return vData{ isPressed ? 1LL : 0LL };
#endif
        };

    registry[L"KBD_RESTORE"] = [=](const std::vector<vData>&) -> vData {
        return vData{ true };
        };
}


OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {
    RegisterKeyboardFunctions(registry);
}