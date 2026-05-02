#include "../../OliEngine.hpp"
#include <windows.h>
#include <vector>
#include <map>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

// Trebuie să inițializăm GDI+ o singură dată
bool g_GdiPlusStarted = false;
ULONG_PTR g_GdiPlusToken;

void StartGDIPlus() {
    if (!g_GdiPlusStarted) {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&g_GdiPlusToken, &gdiplusStartupInput, NULL);
        g_GdiPlusStarted = true;
    }
}

#define OLI_EXPORT extern "C" __declspec(dllexport)


using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

struct Sprite {
    std::vector<unsigned int> pixels;
    int width;
    int height;
};

std::map<int, Sprite> g_SpriteLib;
int g_NextSpriteId = 1;

inline double toDouble(const vData& v) {
    if (std::holds_alternative<double>(v.value)) return std::get<double>(v.value);
    if (std::holds_alternative<long long>(v.value)) return static_cast<double>(std::get<long long>(v.value));
    return 0.0;
}

inline uintptr_t toPointer(const vData& v) {
    if (std::holds_alternative<long long>(v.value)) return (uintptr_t)std::get<long long>(v.value);
    if (std::holds_alternative<double>(v.value)) return (uintptr_t)std::get<double>(v.value);
    return 0;
}

OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {

    // SPRITE_LOAD("cale/catre/imagine.bmp") -> returnează ID-ul sprite-ului
    registry[L"SPRITE_LOAD"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty() || !std::holds_alternative<std::wstring>(args[0].value)) return vData{ -1LL };

        StartGDIPlus(); // Ne asigurăm că GDI+ e pornit
        std::wstring path = std::get<std::wstring>(args[0].value);

        Bitmap* bitmap = Bitmap::FromFile(path.c_str());
        if (!bitmap || bitmap->GetLastStatus() != Ok) {
            delete bitmap;
            return vData{ -1LL };
        }

        Sprite s;
        s.width = bitmap->GetWidth();
        s.height = bitmap->GetHeight();
        s.pixels.resize(s.width * s.height);

        // Citim pixelii direct
        for (int y = 0; y < s.height; y++) {
            for (int x = 0; x < s.width; x++) {
                Color pixelColor;
                bitmap->GetPixel(x, y, &pixelColor);

                // Stocăm formatul ARGB (A = Alpha, R = Red, G = Green, B = Blue)
                // GDI+ returnează culorile foarte comod
                s.pixels[y * s.width + x] = pixelColor.GetValue();
            }
        }

        delete bitmap;
        int id = g_NextSpriteId++;
        g_SpriteLib[id] = s;
        return vData{ (long long)id };
        };

    // SPRITE_DRAW(ptr, canvasW, canvasH, spriteId, x, y, transparentColor)
    registry[L"SPRITE_DRAW"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 6) return vData{ 0LL };

        unsigned int* canvas = (unsigned int*)toPointer(args[0]);
        int cW = (int)toDouble(args[1]);
        int cH = (int)toDouble(args[2]);
        int sId = (int)toDouble(args[3]);
        int posX = (int)toDouble(args[4]);
        int posY = (int)toDouble(args[5]);

        if (g_SpriteLib.find(sId) == g_SpriteLib.end() || !canvas) return vData{ 0LL };
        const Sprite& s = g_SpriteLib[sId];

        for (int y = 0; y < s.height; y++) {
            for (int x = 0; x < s.width; x++) {
                int tx = posX + x;
                int ty = posY + y;

                if (tx >= 0 && tx < cW && ty >= 0 && ty < cH) {
                    unsigned int argb = s.pixels[y * s.width + x];
                    unsigned char a = (argb >> 24) & 0xFF; // Extragem Alpha

                    if (a == 255) {
                        // Pixel complet opac
                        canvas[ty * cW + tx] = argb & 0xFFFFFF;
                    }
                    else if (a > 0) {
                        // Alpha Blending (Amestecăm culorile)
                        unsigned int bg = canvas[ty * cW + tx];

                        unsigned char r = ((argb >> 16) & 0xFF) * a / 255 + ((bg >> 16) & 0xFF) * (255 - a) / 255;
                        unsigned char g = ((argb >> 8) & 0xFF) * a / 255 + ((bg >> 8) & 0xFF) * (255 - a) / 255;
                        unsigned char b = (argb & 0xFF) * a / 255 + (bg & 0xFF) * (255 - a) / 255;

                        canvas[ty * cW + tx] = (r << 16) | (g << 8) | b;
                    }
                }
            }
        }
        return vData{ 1LL };
        };

    registry[L"SPRITE_SIZE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ 0LL };

        int sId = (int)toDouble(args[0]);
        int newW = (int)toDouble(args[1]);
        int newH = (int)toDouble(args[2]);

        if (g_SpriteLib.find(sId) == g_SpriteLib.end() || newW <= 0 || newH <= 0)
            return vData{ 0LL };

        Sprite& s = g_SpriteLib[sId];
        std::vector<unsigned int> resizedPixels;
        resizedPixels.resize(newW * newH);

        // Calculăm raportul de scalare
        double x_ratio = (double)s.width / newW;
        double y_ratio = (double)s.height / newH;

        for (int y = 0; y < newH; y++) {
            for (int x = 0; x < newW; x++) {
                // Găsim pixelul corespondent în imaginea originală
                int oldX = (int)(x * x_ratio);
                int oldY = (int)(y * y_ratio);
                resizedPixels[y * newW + x] = s.pixels[oldY * s.width + oldX];
            }
        }

        // Înlocuim datele vechi cu cele noi
        s.pixels = std::move(resizedPixels);
        s.width = newW;
        s.height = newH;

        return vData{ 1LL };
        };
}