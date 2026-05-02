#include "../src/OliEngine.hpp"
#include <unordered_map>
#include <functional>
#include <vector>
#include <chrono>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <map>
#endif

#ifndef _WIN32
#include <X11/Xlib.h>
#include <X11/keysym.h>

// Avem nevoie de display-ul deschis în oli_canvas. 
// Dacă nu îl avem global, îl deschidem local pentru interogare.
void sync_linux_keys_x11() {
    Display* d = XOpenDisplay(NULL);
    if (!d) return;

    char keys_return[32];
    XQueryKeymap(d, keys_return);

    // Mapare coduri Virtual Key (Windows) -> KeyCodes (X11)
    // Săgețile în X11 sunt de obicei:
    // Left: 113, Up: 111, Right: 114, Down: 116, Space: 65, Q: 24

    auto check_key = [&](int x11_keycode, int win_vk) {
        bool pressed = keys_return[x11_keycode >> 3] & (1 << (x11_keycode & 7));
        if (pressed) {
            g_linuxKeyMap[win_vk] = std::chrono::steady_clock::now();
        }
        };

    // Interogăm tastele specifice
    check_key(113, 37); // Left
    check_key(114, 39); // Right
    check_key(111, 38); // Up
    check_key(116, 40); // Down
    check_key(65, 32);  // Space
    check_key(24, 81);  // Q (pentru ieșire)

    XCloseDisplay(d);
}
#endif



#ifndef _WIN32
// --- LOGICĂ LINUX PENTRU SIMULARE ASYNC ---
static std::map<int, std::chrono::steady_clock::time_point> g_linuxKeyMap;

static struct termios oldt; // O mutăm aici pentru a fi accesibilă global în plugin

static void kb_init() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    tcgetattr(STDIN_FILENO, &oldt); // Salvăm setările originale
    struct termios newt = oldt;

    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}
void kb_restore() {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}
// Citim tot ce e în buffer și actualizăm „tabela de stări”
void sync_linux_keys() {
    kb_init();
    unsigned char ch;
    while (read(STDIN_FILENO, &ch, 1) > 0) {
        if (ch == 27) { // Start secvență ESC (săgeți)
            unsigned char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
                if (seq[0] == '[') {
                    int vk = 0;
                    switch (seq[1]) {
                    case 'A': vk = 38; break; // UP
                    case 'B': vk = 40; break; // DOWN
                    case 'C': vk = 39; break; // RIGHT
                    case 'D': vk = 37; break; // LEFT
                    }
                    if (vk) g_linuxKeyMap[vk] = std::chrono::steady_clock::now();
                }
            }
        }
        else {
            // Mapăm Space (32) sau litere
            g_linuxKeyMap[(int)ch] = std::chrono::steady_clock::now();
        }
    }
}
#endif

long long asInt(const vData& data) {
    if (std::holds_alternative<long long>(data.value)) return std::get<long long>(data.value);
    if (std::holds_alternative<double>(data.value)) return static_cast<long long>(std::get<double>(data.value));
    return 0;
}

void RegisterKeyboardFunctions(std::unordered_map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry) {

    // GET_KEY() - Rămâne pentru compatibilitate (event-based)
    registry[L"GET_KEY"] = [=](const std::vector<vData>& a) -> vData {
#ifdef _WIN32
        if (!_kbhit()) return vData{ 0LL };
        int ch = _getch();
        if (ch == 0 || ch == 224) {
            ch = _getch();
            switch (ch) {
            case 72: return vData{ 1001LL }; // UP
            case 80: return vData{ 1002LL }; // DOWN
            case 75: return vData{ 1003LL }; // LEFT
            case 77: return vData{ 1004LL }; // RIGHT
            }
        }
        return vData{ (long long)ch };
#else
        sync_linux_keys();
        // Returnăm ultima tastă văzută (simplificat pentru Linux)
        if (g_linuxKeyMap.empty()) return vData{ 0LL };
        return vData{ (long long)g_linuxKeyMap.rbegin()->first };
#endif
        };

    // KEY_STATE(vk) - ACUM MERGE ȘI PE LINUX!
    registry[L"KEY_STATE"] = [=](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0LL };
        int vk = (int)asInt(a[0]);

#ifdef _WIN32
        short state = GetAsyncKeyState(vk);
        return vData{ (state & 0x8000) ? 1LL : 0LL };
#else
        // Metoda sigură pentru X11 (fără delay de terminal)
        Display* d = XOpenDisplay(NULL);
        if (!d) return vData{ 0LL };

        char keys[32];
        XQueryKeymap(d, keys);

        // Convertim VK în KeyCode X11 (simplificat pentru test)
        int kc = 0;
        if (vk == 37) kc = XKeysymToKeycode(d, XK_Left);
        if (vk == 39) kc = XKeysymToKeycode(d, XK_Right);
        if (vk == 38) kc = XKeysymToKeycode(d, XK_Up);
        if (vk == 40) kc = XKeysymToKeycode(d, XK_Down);
        if (vk == 32) kc = XKeysymToKeycode(d, XK_space);
        if (vk == 81) kc = XKeysymToKeycode(d, XK_q);

        bool isPressed = keys[kc >> 3] & (1 << (kc & 7));
        XCloseDisplay(d);

        return vData{ isPressed ? 1LL : 0LL };
#endif
        };

    registry[L"KBD_RESTORE"] = [=](const std::vector<vData>& a) -> vData {
#ifndef _WIN32
        kb_restore();
        return vData{ true };
#else
        return vData{ true }; // Pe Windows nu e nevoie de asta
#endif
        };
}