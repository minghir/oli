#include "../../OliEngine.hpp"
#include <unordered_map>
#include <string>
#include <functional>
#include <iostream>
#include <vector>
#include <variant>

#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#include <windows.h>
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#include <unistd.h>
#endif

// --- HELPER SIGURANȚĂ DATE ---
long long asInt(const vData& data) {
    if (std::holds_alternative<long long>(data.value))
        return std::get<long long>(data.value);
    if (std::holds_alternative<double>(data.value))
        return static_cast<long long>(std::get<double>(data.value));
    return 0;
}

// --- LOGICĂ BUFFER (g_pizda) ---
struct ConsoleBuffer {
    int width = 0;
    int height = 0;

#if defined(_WIN32)
    std::vector<CHAR_INFO> buffer;
    HANDLE hOut = nullptr;
#else
    struct Cell { wchar_t c; int col; };
    std::vector<Cell> buffer;
#endif

    void init(int w, int h) {
        width = w;
        height = h;
#if defined(_WIN32)
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        buffer.assign(w * h, { 0 });
#else
        buffer.assign(w * h, { L' ', 7 });
#endif
        clear();
    }

    void clear() {
#if defined(_WIN32)
        for (auto& cell : buffer) {
            cell.Char.UnicodeChar = L' ';
            cell.Attributes = 7;
        }
#else
        for (auto& cell : buffer) {
            cell.c = L' ';
            cell.col = 7;
        }
#endif
    }

    void put(int x, int y, wchar_t c, int color) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
#if defined(_WIN32)
            buffer[y * width + x].Char.UnicodeChar = c;
            buffer[y * width + x].Attributes = (WORD)color;
#else
            buffer[y * width + x] = { c, color };
#endif
        }
    }

    void present() {
#if defined(_WIN32)
        COORD bufferSize = { (short)width, (short)height };
        COORD bufferCoord = { 0, 0 };
        SMALL_RECT writeRegion = { 0, 0, (short)(width - 1), (short)(height - 1) };
        WriteConsoleOutputW(hOut, buffer.data(), bufferSize, bufferCoord, &writeRegion);
#else
        // Implementare ANSI pentru Linux
        std::wstring out = L"\033[H"; // Cursor la Home (0,0)
        int lastCol = -1;

        for (int i = 0; i < (int)buffer.size(); ++i) {
            if (buffer[i].col != lastCol) {
                // Schimbare culoare ANSI
                std::string ansi = "\033[0m"; // Default
                switch (buffer[i].col) {
                case 12: ansi = "\033[31m"; break; // Red
                case 14: ansi = "\033[33m"; break; // Yellow
                case 11: ansi = "\033[36m"; break; // Cyan
                case 10: ansi = "\033[32m"; break; // Green
                case 8:  ansi = "\033[90m"; break; // Dark Gray
                }
                for (char ch : ansi) out += (wchar_t)ch;
                lastCol = buffer[i].col;
            }
            out += buffer[i].c;
            if ((i + 1) % width == 0) out += L"\n";
        }
        std::wcout << out << std::flush;
#endif
        clear(); // Pregătim bufferul pentru frame-ul următor
    }
};

static ConsoleBuffer g_pizda;

// --- REGISTRE FUNCȚII ---
using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

void RegisterConsoleFunctions(PluginRegistry& registry) {

    registry[L"CON_SCREEN"] = [](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ false };
        int w = (int)asInt(a[0]);
        int h = (int)asInt(a[1]);
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        COORD size = { (short)w, (short)h };
        SMALL_RECT tmpRect = { 0, 0, 1, 1 };
        SetConsoleWindowInfo(hOut, TRUE, &tmpRect);
        SetConsoleScreenBufferSize(hOut, size);
        SMALL_RECT finalRect = { 0, 0, (short)(w - 1), (short)(h - 1) };
        SetConsoleWindowInfo(hOut, TRUE, &finalRect);
        return vData{ true };
#else
        // Pe Linux trimitem un escape code pentru resize terminal (nu merge peste tot)
        std::wcout << L"\033[8;" << h << L";" << w << L"t";
        return vData{ true };
#endif
        };

    registry[L"CON_CLS"] = [](const std::vector<vData>&) -> vData {
#ifdef _WIN32
        system("cls");
#else
        std::wcout << L"\033[2J\033[H";
#endif
        return vData{ true };
        };

    registry[L"CURSOR"] = [](const std::vector<vData>& a) -> vData {
        bool show = true;
        if (!a.empty()) {
            if (std::holds_alternative<bool>(a[0].value)) show = std::get<bool>(a[0].value);
            else show = asInt(a[0]) != 0;
        }
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO ci;
        GetConsoleCursorInfo(hOut, &ci);
        ci.bVisible = show;
        SetConsoleCursorInfo(hOut, &ci);
#else
        std::wcout << (show ? L"\033[?25h" : L"\033[?25l");
#endif
        return vData{ true };
        };

    registry[L"DB_INIT"] = [](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ false };
        g_pizda.init((int)asInt(a[0]), (int)asInt(a[1]));
        return vData{ true };
        };

    registry[L"DB_PUT"] = [](const std::vector<vData>& a) -> vData {
        if (a.size() < 3) return vData{ false };

        int x = (int)asInt(a[0]);
        int y = (int)asInt(a[1]);
        int col = (a.size() > 3) ? (int)asInt(a[3]) : 7;

        std::wstring text;

        // Convertim automat orice primim în text (String, Int sau Double)
        if (std::holds_alternative<std::wstring>(a[2].value)) {
            text = std::get<std::wstring>(a[2].value);
        }
        else if (std::holds_alternative<long long>(a[2].value)) {
            text = std::to_wstring(std::get<long long>(a[2].value));
        }
        else if (std::holds_alternative<double>(a[2].value)) {
            text = std::to_wstring((int)std::get<double>(a[2].value));
        }

        if (!text.empty()) {
            // Punem tot string-ul în buffer, caracter cu caracter
            for (int i = 0; i < (int)text.length(); ++i) {
                g_pizda.put(x + i, y, text[i], col);
            }
            return vData{ true };
        }
        return vData{ false };
        };

    registry[L"DB_PRESENT"] = [](const std::vector<vData>&) -> vData {
        g_pizda.present();
        return vData{ true };
        };
}

OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {
    RegisterConsoleFunctions(registry);
}