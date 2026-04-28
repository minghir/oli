#include "../src/OliEngine.hpp"
#include <map>
#include <functional>
#include <vector>
#include <conio.h> // Specific pentru Windows/MINGW (kbhit, getch)
#include <windows.h> // Pentru detectarea stării tastelor (săgeți)

void RegisterKeyboardFunctions(std::map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry) {

    // IS_KEY_PRESSED() -> Returnează 1 dacă o tastă a fost apăsată (non-blocking)
    registry[L"IS_KEY_PRESSED"] = [=](const std::vector<vData>& a) -> vData {
        return vData{ _kbhit() ? 1LL : 0LL };
    };

    // GET_KEY() -> Citește tasta (blocking). Gestionează și codurile extinse (săgeți)
    registry[L"GET_KEY"] = [=](const std::vector<vData>& a) -> vData {
        if (!_kbhit()) return vData{ 0LL };

        int ch = _getch();
        
        // Dacă este cod extins (0 sau 224), mai citim o dată pentru a lua codul tastei
        if (ch == 0 || ch == 224) {
            ch = _getch();
            // Mapăm codurile de săgeți la valori standardizate pentru oli#
            switch(ch) {
                case 72: return vData{ 1001LL }; // UP
                case 80: return vData{ 1002LL }; // DOWN
                case 75: return vData{ 1003LL }; // LEFT
                case 77: return vData{ 1004LL }; // RIGHT
            }
        }
        
        return vData{ static_cast<long long>(ch) };
    };

    // KEY_STATE(vk_code) -> Verifică starea unei taste specifice (via Virtual Key Codes)
    // Util pentru jocuri: KEY_STATE(32) verifică Space-ul
    registry[L"KEY_STATE"] = [=](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0LL };
        int vk = static_cast<int>(std::get<long long>(a[0].value));
        
        // GetAsyncKeyState verifică dacă tasta este apăsată în prezent
        short state = GetAsyncKeyState(vk);
        return vData{ (state & 0x8000) ? 1LL : 0LL };
    };
}