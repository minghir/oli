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
// --- LOGICĂ LINUX PENTRU SIMULARE ASYNC ---
static std::map<int, std::chrono::steady_clock::time_point> g_linuxKeyMap;

static void kb_init() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    struct termios newt;
    tcgetattr(STDIN_FILENO, &newt);
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
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
        sync_linux_keys();
        auto it = g_linuxKeyMap.find(vk);
        if (it != g_linuxKeyMap.end()) {
            auto now = std::chrono::steady_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
            // Dacă am văzut tasta în ultimele 100ms, o considerăm „apăsată”
            // Terminalul trimite repeat-uri cam la 30-50ms
            if (diff < 100) return vData{ 1LL };
        }
        return vData{ 0LL };
#endif
        };
}