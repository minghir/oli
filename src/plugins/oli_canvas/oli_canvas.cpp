#include "../../OliEngine.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>

// --- MACRO DEFINIȚII ȘI INCLUDE-URI SPECIFICE ---
#ifdef _WIN32
#include <windows.h>
#define OLI_EXPORT extern "C" __declspec(dllexport)
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <string.h> 
#define OLI_EXPORT extern "C"
#endif

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

struct CanvasState {
    int width = 0;
    int height = 0;
    void* pBits = nullptr;
#ifdef _WIN32
    HWND hwnd = nullptr;
    HDC hdcMem = nullptr;
    HBITMAP hbmMem = nullptr;
#else
    Display* display = nullptr;
    Window window;
    GC gc;
    XImage* ximage = nullptr;
#endif
} g_Canvas;

inline double toDouble(const vData& v) {
    if (std::holds_alternative<double>(v.value)) return std::get<double>(v.value);
    if (std::holds_alternative<long long>(v.value)) return static_cast<double>(std::get<long long>(v.value));
    return 0.0;
}

#ifdef _WIN32
LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (hwnd == g_Canvas.hwnd) g_Canvas.hwnd = nullptr;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
#endif

OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {

    registry[L"CAN_INIT"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ 0LL };

        int w = static_cast<int>(toDouble(args[0]));
        int h = static_cast<int>(toDouble(args[1]));
        std::wstring title = std::get<std::wstring>(args[2].value);
        g_Canvas.width = w;
        g_Canvas.height = h;

#ifdef _WIN32
        HINSTANCE hInst = GetModuleHandle(NULL);

        static bool classRegistered = false;
        if (!classRegistered) {
            WNDCLASSW wc = { 0 };
            wc.lpfnWndProc = CanvasWndProc;
            wc.hInstance = hInst;
            wc.lpszClassName = L"OliCanvasClass";
            wc.hCursor = LoadCursor(NULL, IDC_ARROW);
            wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
            RegisterClassW(&wc);
            classRegistered = true;
        }

        RECT rc = { 0, 0, w, h };
        DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
        AdjustWindowRect(&rc, dwStyle, FALSE);

        g_Canvas.hwnd = CreateWindowExW(0, L"OliCanvasClass", title.c_str(),
            dwStyle, CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInst, NULL);

        if (!g_Canvas.hwnd) return vData{ 0LL };

        HDC hdc = GetDC(g_Canvas.hwnd);
        g_Canvas.hdcMem = CreateCompatibleDC(hdc);

        BITMAPINFO bmi = { 0 };
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;

        g_Canvas.hbmMem = CreateDIBSection(g_Canvas.hdcMem, &bmi, DIB_RGB_COLORS, &g_Canvas.pBits, NULL, 0);
        SelectObject(g_Canvas.hdcMem, g_Canvas.hbmMem);
        ReleaseDC(g_Canvas.hwnd, hdc);
#else
        // Implementare Linux (X11) - neatinsă dacă ești pe Windows
        g_Canvas.display = XOpenDisplay(NULL);
        if (!g_Canvas.display) return vData{ 0LL };
        int screen = DefaultScreen(g_Canvas.display);
        g_Canvas.window = XCreateSimpleWindow(g_Canvas.display, RootWindow(g_Canvas.display, screen),
            0, 0, w, h, 1, 0, 0);
        XMapWindow(g_Canvas.display, g_Canvas.window);
        g_Canvas.gc = DefaultGC(g_Canvas.display, screen);
        g_Canvas.pBits = malloc(w * h * 4);
        g_Canvas.ximage = XCreateImage(g_Canvas.display, DefaultVisual(g_Canvas.display, screen),
            24, ZPixmap, 0, (char*)g_Canvas.pBits, w, h, 32, 0);
#endif
        return vData{ 1LL };
        };

    // --- CAN_PRESENT ---
    registry[L"CAN_PRESENT"] = [](const std::vector<vData>&) -> vData {
#ifdef _WIN32
        if (g_Canvas.hwnd && g_Canvas.hdcMem) {
            HDC hdc = GetDC(g_Canvas.hwnd);
            BitBlt(hdc, 0, 0, g_Canvas.width, g_Canvas.height, g_Canvas.hdcMem, 0, 0, SRCCOPY);
            ReleaseDC(g_Canvas.hwnd, hdc);
            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg); DispatchMessage(&msg);
            }
        }
#else
        if (g_Canvas.display && g_Canvas.ximage) {
            XPutImage(g_Canvas.display, g_Canvas.window, g_Canvas.gc, g_Canvas.ximage,
                0, 0, 0, 0, g_Canvas.width, g_Canvas.height);
            XFlush(g_Canvas.display);
        }
#endif
        return vData{ 1LL };
        };

    // --- FUNCȚIILE DE DESENARE RĂMÂN IDENTICE! ---
    // CAN_PUT(x, y, color) - Color format: 0xRRGGBB
    registry[L"CAN_PUT"] = [](const std::vector<vData>& args) -> vData {
        // Verificăm dacă avem destule argumente și dacă buffer-ul de pixeli e gata
        if (args.size() < 3 || !g_Canvas.pBits) return vData{ 0LL };

        // Folosim toDouble pentru a evita "bad variant access"
        int x = static_cast<int>(toDouble(args[0]));
        int y = static_cast<int>(toDouble(args[1]));

        // Pentru culoare, facem cast la unsigned int după conversia în double
        unsigned int color = static_cast<unsigned int>(toDouble(args[2]));

        // Bounds checking (foarte important să nu scriem în afara memoriei!)
        if (x >= 0 && x < g_Canvas.width && y >= 0 && y < g_Canvas.height) {
            unsigned int* pixels = (unsigned int*)g_Canvas.pBits;
            pixels[y * g_Canvas.width + x] = color;
        }
        return vData{ 1LL };
        };

    registry[L"CAN_CLS"] = [](const std::vector<vData>& args) -> vData {
        if (!g_Canvas.pBits) return vData{ 0LL };

        unsigned int color = 0;
        if (!args.empty()) color = static_cast<unsigned int>(toDouble(args[0]));

        // Dacă e negru (0), folosim memset care e instant
        if (color == 0) {
            memset(g_Canvas.pBits, 0, g_Canvas.width * g_Canvas.height * 4);
        }
        else {
            unsigned int* pixels = (unsigned int*)g_Canvas.pBits;
            std::fill(pixels, pixels + (g_Canvas.width * g_Canvas.height), color);
        }
        return vData{ 1LL };
        };

    registry[L"CAN_NOISE"] = [](const std::vector<vData>&) -> vData {
        if (!g_Canvas.pBits) return vData{ 0LL };

        unsigned int* pixels = (unsigned int*)g_Canvas.pBits;
        int total = g_Canvas.width * g_Canvas.height;

        for (int i = 0; i < total; i++) {
            unsigned char c = rand() % 256;
            pixels[i] = RGB(c, c, c);
        }
        return vData{ 1LL };
        };

    registry[L"CAN_RECT"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 5 || !g_Canvas.pBits) return vData{ 0LL };

        int rx = static_cast<int>(toDouble(args[0]));
        int ry = static_cast<int>(toDouble(args[1]));
        int rw = static_cast<int>(toDouble(args[2]));
        int rh = static_cast<int>(toDouble(args[3]));
        unsigned int color = static_cast<unsigned int>(toDouble(args[4]));

        unsigned int* pixels = (unsigned int*)g_Canvas.pBits;

        // Clipping: Ne asigurăm că nu desenăm în afara ferestrei (evităm crash-ul)
        int x1 = std::max<int>(0, rx);
        int y1 = std::max<int>(0, ry);
        int x2 = std::min<int>(g_Canvas.width, rx + rw);
        int y2 = std::min<int>(g_Canvas.height, ry + rh);

        for (int y = y1; y < y2; ++y) {
            // Calculăm începutul rândului o singură dată per linie (optimizare)
            unsigned int* row = pixels + (y * g_Canvas.width);
            for (int x = x1; x < x2; ++x) {
                row[x] = color;
            }
        }
        return vData{ 1LL };
        };

    registry[L"CAN_CIRCLE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4 || !g_Canvas.pBits) return vData{ 0LL };

        int xm = static_cast<int>(toDouble(args[0]));
        int ym = static_cast<int>(toDouble(args[1]));
        int r = static_cast<int>(toDouble(args[2]));
        unsigned int color = static_cast<unsigned int>(toDouble(args[3]));

        unsigned int* pixels = (unsigned int*)g_Canvas.pBits;
        int x = -r, y = 0, err = 2 - 2 * r;

        do {
            // Desenăm cele 4 segmente/puncte simetrice
            auto plot = [&](int px, int py) {
                if (px >= 0 && px < g_Canvas.width && py >= 0 && py < g_Canvas.height)
                    pixels[py * g_Canvas.width + px] = color;
                };

            plot(xm - x, ym + y);
            plot(xm - y, ym - x);
            plot(xm + x, ym - y);
            plot(xm + y, ym + x);

            r = err;
            if (r <= y) err += ++y * 2 + 1;
            if (r > x || err > y) err += ++x * 2 + 1;
        } while (x < 0);

        return vData{ 1LL };
        };


    registry[L"CAN_CIRCLE_FILL"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4 || !g_Canvas.pBits) return vData{ 0LL };

        int xm = static_cast<int>(toDouble(args[0]));
        int ym = static_cast<int>(toDouble(args[1]));
        int r = static_cast<int>(toDouble(args[2]));
        unsigned int color = static_cast<unsigned int>(toDouble(args[3]));

        unsigned int* pixels = (unsigned int*)g_Canvas.pBits;

        for (int y = -r; y <= r; y++) {
            for (int x = -r; x <= r; x++) {
                if (x * x + y * y <= r * r) {
                    int px = xm + x;
                    int py = ym + y;
                    if (px >= 0 && px < g_Canvas.width && py >= 0 && py < g_Canvas.height) {
                        pixels[py * g_Canvas.width + px] = color;
                    }
                }
            }
        }
        return vData{ 1LL };
        };

    registry[L"CAN_GET_PTR"] = [](const std::vector<vData>&) -> vData {
        // Returnăm adresa pointerului ca un număr (long long)
        return vData{ (long long)g_Canvas.pBits };
        };

    registry[L"CAN_GET_WIDTH"] = [](const std::vector<vData>&) -> vData { return vData{ (long long)g_Canvas.width }; };
    registry[L"CAN_GET_HEIGHT"] = [](const std::vector<vData>&) -> vData { return vData{ (long long)g_Canvas.height }; };

    registry[L"CAN_CLOSE"] = [](const std::vector<vData>&) -> vData {
        if (g_Canvas.hwnd) {
            // Distrugem obiectele GDI
            if (g_Canvas.hbmMem) DeleteObject(g_Canvas.hbmMem);
            if (g_Canvas.hdcMem) DeleteDC(g_Canvas.hdcMem);

            // Închidem fereastra
            DestroyWindow(g_Canvas.hwnd);

            // Resetăm starea globală
            g_Canvas.hwnd = nullptr;
            g_Canvas.hdcMem = nullptr;
            g_Canvas.hbmMem = nullptr;
            g_Canvas.pBits = nullptr;
        }
        return vData{ 1LL };
        };
}