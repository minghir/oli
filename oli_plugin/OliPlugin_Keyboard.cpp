#include "../src/OliEngine.hpp"
#include <map>
#include <functional>
#include <vector>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#ifndef _WIN32

static void kb_init() {
    static bool initialized = false;
    static struct termios oldt;

    if (initialized) return;
    initialized = true;

    struct termios newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    newt.c_lflag &= ~(ICANON | ECHO);  // no buffering, no echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // non-blocking read
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

int kbhit() {
    kb_init();
    unsigned char ch;
    int n = read(STDIN_FILENO, &ch, 1);
    if (n > 0) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

int getch() {
    kb_init();
    unsigned char ch;
    while (read(STDIN_FILENO, &ch, 1) <= 0) {
        usleep(1000); // 1ms
    }
    return ch;
}

#endif


void RegisterKeyboardFunctions(std::unordered_map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry) {

    // IS_KEY_PRESSED()
    registry[L"IS_KEY_PRESSED"] = [=](const std::vector<vData>& a) -> vData {
#ifdef _WIN32
        return vData{ _kbhit() ? 1LL : 0LL };
#else
        return vData{ kbhit() ? 1LL : 0LL };
#endif
        };

    // GET_KEY()
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
        if (!kbhit()) return vData{ 0LL };
        int ch = getch();
        return vData{ (long long)ch };
#endif
        };

    // KEY_STATE()
    registry[L"KEY_STATE"] = [=](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0LL };

#ifdef _WIN32
        int vk = (int)std::get<long long>(a[0].value);
        short state = GetAsyncKeyState(vk);
        return vData{ (state & 0x8000) ? 1LL : 0LL };
#else
        // Linux does NOT support async key state polling
        // Returnăm 0 ca fallback
        return vData{ 0LL };
#endif
        };
}
