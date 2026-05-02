#include "../../OliEngine.hpp"
#include <windows.h>
#ifndef PI
#define PI 3.14159265358979323846
#endif


#ifdef _WIN32
#include <windows.h>
#else
#include <string.h>
#define RGB(r,g,b) ((unsigned int)((b) | ((g) << 8) | ((r) << 16)))
// Dacă folosești GetRValue, GetGValue, etc în sprites:
#define GetRValue(rgb) ((unsigned char)(((rgb) >> 16) & 0xff))
#define GetGValue(rgb) ((unsigned char)(((rgb) >> 8) & 0xff))
#define GetBValue(rgb) ((unsigned char)((rgb) & 0xff))
#endif

#define OLI_EXPORT extern "C" __declspec(dllexport)


using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

// Helper pentru a extrage adresa de memorie (pointerul) în siguranță
inline uintptr_t toPointer(const vData& v) {
    if (std::holds_alternative<long long>(v.value))
        return (uintptr_t)std::get<long long>(v.value);

    if (std::holds_alternative<double>(v.value))
        return (uintptr_t)std::get<double>(v.value);

    return 0;
}

// Helper pentru conversia variantului vData în double
inline double toDouble(const vData& v) {
    if (std::holds_alternative<double>(v.value))
        return std::get<double>(v.value);

    if (std::holds_alternative<long long>(v.value))
        return static_cast<double>(std::get<long long>(v.value));

    return 0.0;
}

OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {
    registry[L"FRACT_JULIA"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 6) return vData{ 0LL };

        // Folosim toPointer în loc de std::get direct pentru args[0]
        unsigned int* pixels = (unsigned int*)toPointer(args[0]);

        int w = (int)toDouble(args[1]);
        int h = (int)toDouble(args[2]);
        double cX = toDouble(args[3]);
        double cY = toDouble(args[4]);
        int maxIter = (int)toDouble(args[5]);

        if (!pixels) return vData{ 0LL };

        // ... restul algoritmului rămâne la fel ...
        for (int y = 0; y < h; y++) {
            double zy_init = 1.0 * (2.0 * y / h - 1.0);
            for (int x = 0; x < w; x++) {
                double zx = 1.5 * (2.0 * x / w - 1.0);
                double zy = zy_init;
                int i = 0;
                while (i < maxIter && (zx * zx + zy * zy) < 4.0) {
                    double xtemp = zx * zx - zy * zy + cX;
                    zy = 2.0 * zx * zy + cY;
                    zx = xtemp;
                    i++;
                }
                pixels[y * w + x] = (i == maxIter) ? 0 : (unsigned int)((i * 255 / maxIter) << 8 | 128);
            }
        }
        return vData{ 1LL };
        };

    registry[L"FRACT_MANDEL"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 6 || !toPointer(args[0])) return vData{ 0LL };

        unsigned int* pixels = (unsigned int*)toPointer(args[0]);
        int w = (int)toDouble(args[1]);
        int h = (int)toDouble(args[2]);
        double zoom = toDouble(args[3]);
        double moveX = toDouble(args[4]);
        double moveY = toDouble(args[5]);
        int maxIter = 64;

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                // Transformăm coordonatele pixelilor în plan complex
                double pr = 1.5 * (x - w / 2) / (0.5 * zoom * w) + moveX;
                double pi = (y - h / 2) / (0.5 * zoom * h) + moveY;
                double zr = 0, zi = 0;
                int i = 0;

                while (i < maxIter && (zr * zr + zi * zi) < 4.0) {
                    double temp = zr * zr - zi * zi + pr;
                    zi = 2.0 * zr * zi + pi;
                    zr = temp;
                    i++;
                }

                // Colorare: Interiorul e negru, exteriorul e un gradient de "foc"
                if (i == maxIter) {
                    pixels[y * w + x] = 0;
                }
                else {
                    unsigned char red = (i * 255 / maxIter);
                    unsigned char green = (i * 128 / maxIter);
                    pixels[y * w + x] = (red << 16) | (green << 8); // RGB foc
                }
            }
        }
        return vData{ 1LL };
        };

    registry[L"FRACT_PLASMA"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4 || !toPointer(args[0])) return vData{ 0LL };

        unsigned int* pixels = (unsigned int*)toPointer(args[0]);
        int w = (int)toDouble(args[1]);
        int h = (int)toDouble(args[2]);
        double time = toDouble(args[3]);

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                double v = 0.0;
                double cx = x / (double)w - 0.5;
                double cy = y / (double)h - 0.5;

                // Calcul plasmă bazat pe sinusuri suprapuse
                v += sin((cx * 10.0 + time));
                v += sin((10.0 * (cx * sin(time / 2.0) + cy * cos(time / 3.0)) + time));
                v += sin(sqrt(100.0 * (cx * cx + cy * cy) + 1.0) + time);
                v /= 2.0;

                // Mapare culori: RGB calculat pentru a crea degradeuri de mov/albastru/verde
                unsigned char r = (unsigned char)((sin(v * PI) * 0.5 + 0.5) * 255);
                unsigned char g = (unsigned char)((sin(v * PI + 2.0 * PI / 3.0) * 0.5 + 0.5) * 255);
                unsigned char b = (unsigned char)((sin(v * PI + 4.0 * PI / 3.0) * 0.5 + 0.5) * 255);

                pixels[y * w + x] = (r << 16) | (g << 8) | b;
            }
        }
        return vData{ 1LL };
        };
}