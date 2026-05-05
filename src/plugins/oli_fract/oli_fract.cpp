#include "../../OliEngine.hpp"
#ifdef _WIN32
#include <windows.h>
#define OLI_EXPORT extern "C" __declspec(dllexport)
#else
#include <cmath>
#include <string.h>
#define OLI_EXPORT extern "C"
#define RGB(r,g,b) ((unsigned int)((b) | ((g) << 8) | ((r) << 16)))
// Dacă folosești GetRValue, GetGValue, etc în sprites:
#define GetRValue(rgb) ((unsigned char)(((rgb) >> 16) & 0xff))
#define GetGValue(rgb) ((unsigned char)(((rgb) >> 8) & 0xff))
#define GetBValue(rgb) ((unsigned char)((rgb) & 0xff))
#endif

#include <cmath>
#include <algorithm>

#ifndef PI
#define PI 3.14159265358979323846
#endif


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
		
		registry[L"FX_STARFIELD"] = [](const std::vector<vData>& args) -> vData {
			if (args.size() < 4 || !toPointer(args[0])) return vData{ 0LL };

			unsigned int* pixels = (unsigned int*)toPointer(args[0]);
			int w = (int)toDouble(args[1]);
			int h = (int)toDouble(args[2]);
			double time = toDouble(args[3]);

			// Curățăm ecranul (Black background)
			memset(pixels, 0, w * h * sizeof(unsigned int));

			int numStars = 1000;
			for (int i = 0; i < numStars; i++) {
				// Generăm stele determinist bazat pe index
				double x = cos(i * 452.12) * 1000.0;
				double y = sin(i * 128.45) * 1000.0;
				double z = fmod(i * 10.0 + 2000.0 - fmod(time * 500.0, 2000.0), 2000.0);

				if (z <= 10) continue;

				// Proiecție 3D -> 2D
				int screenX = (int)((x / z) * 200.0 + w / 2);
				int screenY = (int)((y / z) * 200.0 + h / 2);

				// Verificăm limitele ecranului
				if (screenX >= 0 && screenX < w && screenY >= 0 && screenY < h) {
					// Strălucirea stelei scade cu distanța (z)
					unsigned char brightness = (unsigned char)(255 * (1.0 - z / 2000.0));
					
					// Efect de "Blue Shift" pentru viteză (Albastru mai intens în centru)
					unsigned int color = (brightness << 16) | (brightness << 8) | 255;
					
					pixels[screenY * w + screenX] = color;
					
					// Facem stelele mai apropiate să pară mai mari (2x2 pixeli)
					if (z < 500 && screenX + 1 < w && screenY + 1 < h) {
						pixels[screenY * w + screenX + 1] = color;
						pixels[(screenY + 1) * w + screenX] = color;
					}
				}
			}
			return vData{ 1LL };
		};
		
				registry[L"FX_MATRIX"] = [](const std::vector<vData>& args) -> vData {
					if (args.size() < 3 || !toPointer(args[0])) return vData{ 0LL };

					unsigned int* pixels = (unsigned int*)toPointer(args[0]);
					int w = (int)toDouble(args[1]);
					int h = (int)toDouble(args[2]);

					// ───────────────────────────────────────────────
					// 1. SHIFT ÎN JOS (de la y-1 la y)
					// ───────────────────────────────────────────────
					for (int y = h - 1; y > 0; y--) {
						memcpy(&pixels[y * w], &pixels[(y - 1) * w], w * sizeof(unsigned int));
					}

					// ───────────────────────────────────────────────
					// 2. CURĂȚĂM COMPLET LINIA 0 (obligatoriu!)
					//    Fără asta, vezi doar 1 cm sus și restul negru.
					// ───────────────────────────────────────────────
					for (int x = 0; x < w; x++) {
						pixels[x] = 0x000000; // negru
					}

					// ───────────────────────────────────────────────
					// 3. FADE LENT (dâră lungă Matrix)
					// ───────────────────────────────────────────────
					for (int i = w; i < w * h; i++) {  // începem de la linia 1
						unsigned int c = pixels[i];
						if (c == 0) continue;

						unsigned char r = (c >> 16) & 0xFF;
						unsigned char g = (c >> 8) & 0xFF;
						unsigned char b = c & 0xFF;

						// Fade lent → dâră lungă
						unsigned char nr = (r > 20) ? r - 20 : 0;
						unsigned char ng = (g > 5) ? g - 5 : 0;
						unsigned char nb = (b > 20) ? b - 20 : 0;

						pixels[i] = (nr << 16) | (ng << 8) | nb;
					}

					// ───────────────────────────────────────────────
					// 4. GENERARE PICĂTURI NOI PE LINIA 0
					// ───────────────────────────────────────────────
					for (int x = 0; x < w; x += 15) {
						if ((rand() % 100) > 96) {
							// Cap de picătură foarte luminos
							pixels[x] = 0xD0FFD0;
						}
					}

					return vData{ 1LL };
				};
				
			registry[L"FX_MATRIX_ASCII"] = [](const std::vector<vData>& args) -> vData {
				if (args.size() < 3 || !toPointer(args[0])) return vData{ 0LL };

				unsigned int* pixels = (unsigned int*)toPointer(args[0]);
				int w = (int)toDouble(args[1]);
				int h = (int)toDouble(args[2]);

				// 1. Curățăm tot ecranul
				memset(pixels, 0, w * h * sizeof(unsigned int));

				// 2. Stare per coloană
				static std::vector<int> headY, speed, cooldown, length;
				static std::vector<std::vector<char>> stream;

				if ((int)headY.size() != w) {
					headY.resize(w);
					speed.resize(w);
					cooldown.resize(w);
					length.resize(w);
					stream.resize(w);

					for (int x = 0; x < w; x++) {
						headY[x] = rand() % h;
						speed[x] = 2 + rand() % 5;        // viteze lente
						cooldown[x] = speed[x];
						length[x] = 20 + rand() % 30;     // dâră lungă

						stream[x].resize(length[x]);
						for (int i = 0; i < length[x]; i++)
							stream[x][i] = 33 + rand() % 94; // ASCII vizibil
					}
				}

				// 3. Pentru fiecare coloană
				for (int x = 0; x < w; x++) {

					// timer pentru viteză
					cooldown[x]--;
					if (cooldown[x] <= 0) {
						headY[x]++;
						cooldown[x] = speed[x];

						if (headY[x] >= h + length[x]) {
							headY[x] = - (rand() % h);
							speed[x] = 2 + rand() % 5;
							length[x] = 20 + rand() % 30;

							stream[x].resize(length[x]);
							for (int i = 0; i < length[x]; i++)
								stream[x][i] = 33 + rand() % 94;
						}
					}

					// desenăm fluxul ASCII
					for (int i = 0; i < length[x]; i++) {
						int y = headY[x] - i;
						if (y < 0 || y >= h) continue;

						// intensitate în funcție de distanța față de cap
						float t = (float)i / (float)length[x];
						unsigned char g = (unsigned char)(255 * (1.0f - t));
						unsigned char r = g * 0.2f;
						unsigned char b = g * 0.2f;

						unsigned int col = (r << 16) | (g << 8) | b;

						// desenăm pixelul ASCII (punct luminos)
						pixels[y * w + x] = col;

						// schimbăm caracterul periodic (efect de „flicker” Matrix)
						if (rand() % 20 == 0)
							stream[x][i] = 33 + rand() % 94;
					}
				}

				return vData{ 1LL };
			};








				
			registry[L"FX_CYBER_COIL"] = [](const std::vector<vData>& args) -> vData {
				if (args.size() < 4 || !toPointer(args[0])) return vData{ 0LL };

				unsigned int* pixels = (unsigned int*)toPointer(args[0]);
				int w = (int)toDouble(args[1]);
				int h = (int)toDouble(args[2]);
				double time = toDouble(args[3]);

				// Background: Negru curat
				memset(pixels, 0, w * h * sizeof(unsigned int));

				int numPoints = 2000; 
				for (int i = 0; i < numPoints; i++) {
					// Z merge de la 200 (departe) la 0.1 (aproape)
					// Folosim fmod pentru a crea fluxul continuu spre cameră
					double z = 200.0 - fmod(i * 0.1 + time * 40.0, 200.0);
					
					if (z <= 1.0) continue; 

					// UNGHIUL: Se rotește în timp + un twist bazat pe adâncime (z)
					double angle = i * 0.2 + time * 1.5 + (z * 0.02);
					
					// RAZA: O facem să se micșoreze pe măsură ce se apropie pentru a ține punctele pe ecran
					// Sau o lăsăm fixă dar creștem FOV-ul.
					double radius = 100.0 + sin(time + i * 0.01) * 20.0;

					// Perspectiva: cu cât z e mai mic, cu atât punctul e mai mare pe ecran
					double perspective = 300.0 / z; 

					int sx = (int)((cos(angle) * radius) * perspective + w / 2);
					int sy = (int)((sin(angle) * radius) * perspective + h / 2);

					// Verificăm dacă punctul e în interiorul buffer-ului
					if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
						// Culoare: Gradient bazat pe Z (Albastru la distanță, Alb/Cyan aproape)
						unsigned char bright = (unsigned char)(255 * (1.0 - z / 200.0));
						unsigned int col = (bright << 16) | (bright << 8) | 255; 

						pixels[sy * w + sx] = col; 
						// Pixeli adiacenți cu intensitate la jumătate pentru efect de strălucire
						unsigned int halo = (bright/2 << 16) | (bright/2 << 8) | 128;
						if (sx > 0 && sx < w-1 && sy > 0 && sy < h-1) {
							pixels[sy * w + (sx + 1)] += halo;
							pixels[sy * w + (sx - 1)] += halo;
							pixels[(sy + 1) * w + sx] += halo;
							pixels[(sy - 1) * w + sx] += halo;
						}

						// Puncte groase pentru particulele apropiate (Z < 50)
						if (z < 60 && sx + 1 < w && sy + 1 < h) {
							pixels[sy * w + sx + 1] = col;
							pixels[(sy + 1) * w + sx] = col;
							pixels[(sy + 1) * w + sx + 1] = col;
						}
					}
				}
				return vData{ 1LL };
			};
			
			registry[L"FX_3D_SHAPES"] = [](const std::vector<vData>& args) -> vData {
				if (args.size() < 4 || !toPointer(args[0])) return vData{ 0LL };

				unsigned int* pixels = (unsigned int*)toPointer(args[0]);
				int w = (int)toDouble(args[1]);
				int h = (int)toDouble(args[2]);
				double time = toDouble(args[3]);

				// Ștergem ecranul
				memset(pixels, 0, w * h * sizeof(unsigned int));

				auto drawPoint = [&](double x, double y, double z, unsigned int color) {
					// Rotație pe axele Y și X bazată pe timp
					double radY = time * 0.8;
					double radX = time * 0.5;

					// Rotație Y
					double nx = x * cos(radY) + z * sin(radY);
					double nz = -x * sin(radY) + z * cos(radY);
					x = nx; z = nz;

					// Rotație X
					double ny = y * cos(radX) - z * sin(radX);
					nz = y * sin(radX) + z * cos(radX);
					y = ny; z = nz;

					// Proiecție 3D -> 2D (Z-offset de 300 pentru a nu fi în cameră)
					double p = 400.0 / (z + 300.0);
					int sx = (int)(x * p + w / 2);
					int sy = (int)(y * p + h / 2);

					if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
						pixels[sy * w + sx] = color;
					}
				};

				// 1. Randare CUB (muchii din puncte)
				for (double i = -50; i <= 50; i += 5) {
					for (double j = -50; j <= 50; j += 50) {
						for (double k = -50; k <= 50; k += 50) {
							drawPoint(i, j, k, 0x00FF00); // Muchii X
							drawPoint(j, i, k, 0x00FF00); // Muchii Y
							drawPoint(j, k, i, 0x00FF00); // Muchii Z
						}
					}
				}

				// 2. Randare SFERĂ (Point Cloud)
				// O mutăm puțin la dreapta față de cub
				int numPoints = 800;
				for (int i = 0; i < numPoints; i++) {
					double lat = acos(2.0 * i / numPoints - 1.0) - (PI / 2.0);
					double lon = 2.4 * i;

					double r = 60.0; // Raza sferei
					double sx = r * cos(lat) * cos(lon) + 150.0; // Offset +150
					double sy = r * cos(lat) * sin(lon);
					double sz = r * sin(lat);

					drawPoint(sx, sy, sz, 0xFF00FF); // Sfera e roz/magenta
				}

				return vData{ 1LL };
			};
			
			registry[L"FX_COSMIC_AURORA"] = [](const std::vector<vData>& args) -> vData {
    if (args.size() < 4 || !toPointer(args[0])) return vData{ 0LL };

    unsigned int* pixels = (unsigned int*)toPointer(args[0]);
    int w = (int)toDouble(args[1]);
    int h = (int)toDouble(args[2]);
    double t = toDouble(args[3]);

    double time = t * 0.4;          // viteză globală
    double scale = 0.006;           // zoom
    double swirlSpeed = 0.4;        // cât de repede se răsucește
    double flowSpeed = 0.25;        // curgere laterală
    double pulse = 0.5 + 0.5 * sin(t * 0.7); // puls cosmic

    int cx = w / 2;
    int cy = h / 2;

    for (int y = 0; y < h; y++) {
        double dy = (y - cy);

        for (int x = 0; x < w; x++) {
            double dx = (x - cx);

            // coordonate normalizate
            double nx = dx * scale;
            double ny = dy * scale;

            // swirl animat
            double r = sqrt(nx * nx + ny * ny);
            double a = atan2(ny, nx);

            a += swirlSpeed * sin(time + r * 3.0);

            double sx = r * cos(a);
            double sy = r * sin(a);

            // curgere laterală
            sx += sin(time * 0.8 + sy * 3.0) * 0.3;
            sy += cos(time * 0.6 + sx * 2.5) * 0.3;

            // straturi dinamice
            double v1 = sin(sx * 3.0 + time * 1.5);
            double v2 = cos(sy * 4.0 - time * 1.2);
            double v3 = sin((sx + sy) * 2.0 + time * 0.9);

            // energie totală
            double energy = (v1 * 0.5 + v2 * 0.3 + v3 * 0.4);

            // glow radial + puls
            double radial = exp(-r * 1.3);
            double glow = std::max<double>(0.0, energy * radial * (1.0 + pulse));

            if (glow > 1.0) glow = 1.0;

            // paletă dinamică (se schimbă în timp)
            double shift = 0.5 + 0.5 * sin(time * 0.4);

            unsigned char R = (unsigned char)(255.0 * glow * (0.2 + 0.4 * shift));
            unsigned char G = (unsigned char)(255.0 * glow * (0.4 + 0.3 * shift));
            unsigned char B = (unsigned char)(255.0 * glow * (0.8 - 0.5 * shift));

            pixels[y * w + x] = (R << 16) | (G << 8) | B;
        }
    }

    return vData{ 1LL };
};

registry[L"FX_COSMIC_VORTEX"] = [](const std::vector<vData>& args) -> vData {
    if (args.size() < 4 || !toPointer(args[0])) return vData{ 0LL };

    unsigned int* pixels = (unsigned int*)toPointer(args[0]);
    int w = (int)toDouble(args[1]);
    int h = (int)toDouble(args[2]);
    double t = toDouble(args[3]);

    double time = t * 1.2;      // mișcare rapidă
    double scale = 0.005;       // zoom
    double swirl = 1.2;         // răsucire puternică
    double flow = 0.8;          // curgere laterală
    double warp = 0.6;          // deformare
    double pulse = 0.5 + 0.5 * sin(t * 2.0); // puls rapid

    int cx = w / 2;
    int cy = h / 2;

    for (int y = 0; y < h; y++) {
        double dy = (y - cy);

        for (int x = 0; x < w; x++) {
            double dx = (x - cx);

            // coordonate normalizate
            double nx = dx * scale;
            double ny = dy * scale;

            // vortex animat
            double r = sqrt(nx * nx + ny * ny);
            double a = atan2(ny, nx);

            // swirl puternic + animat
            a += swirl * sin(time + r * 4.0);

            // deformare warp
            double sx = r * cos(a);
            double sy = r * sin(a);

            sx += sin(time * 1.5 + sy * 3.0) * warp;
            sy += cos(time * 1.3 + sx * 2.5) * warp;

            // curgere laterală vizibilă
            sx += sin(time * 0.8) * flow;
            sy += cos(time * 0.6) * flow;

            // fractal turbulence
            double v1 = sin(sx * 4.0 + time * 2.0);
            double v2 = cos(sy * 5.0 - time * 1.7);
            double v3 = sin((sx + sy) * 3.0 + time * 1.1);

            double energy = (v1 * 0.5 + v2 * 0.3 + v3 * 0.4);

            // glow radial + puls
            double radial = exp(-r * 1.1);
            double glow = std::max<double>(0.0, energy * radial * (1.0 + pulse));

            if (glow > 1.0) glow = 1.0;

            // paletă dinamică
            double shift = 0.5 + 0.5 * sin(time * 0.3);

            unsigned char R = (unsigned char)(255.0 * glow * (0.3 + 0.4 * shift));
            unsigned char G = (unsigned char)(255.0 * glow * (0.6 + 0.2 * shift));
            unsigned char B = (unsigned char)(255.0 * glow * (1.0 - 0.5 * shift));

            pixels[y * w + x] = (R << 16) | (G << 8) | B;
        }
    }

    return vData{ 1LL };
};

registry[L"FX_SPHERE_TORUS"] = [](const std::vector<vData>& args) -> vData {
    if (args.size() < 4 || !toPointer(args[0])) return vData{ 0LL };

    unsigned int* pixels = (unsigned int*)toPointer(args[0]);
    int w = (int)toDouble(args[1]);
    int h = (int)toDouble(args[2]);
    double t = toDouble(args[3]);

    memset(pixels, 0, w * h * sizeof(unsigned int));

    int cx = w / 2;
    int cy = h / 2;

    double time = t * 0.6;

    // rotații independente
    double rotX = time * 0.7;
    double rotY = time * 0.9;
    double rotZ = time * 0.5;

    auto rotate3D = [&](double& x, double& y, double& z) {
        double x1 = x;
        double y1 = y * cos(rotX) - z * sin(rotX);
        double z1 = y * sin(rotX) + z * cos(rotX);

        double x2 = x1 * cos(rotY) + z1 * sin(rotY);
        double y2 = y1;
        double z2 = -x1 * sin(rotY) + z1 * cos(rotY);

        double x3 = x2 * cos(rotZ) - y2 * sin(rotZ);
        double y3 = x2 * sin(rotZ) + y2 * cos(rotZ);
        double z3 = z2;

        x = x3; y = y3; z = z3;
    };

    // -------------------------
    // SFERĂ COMPLETĂ DIN PUNCTE
    // -------------------------
    double sphereR = 1.0;
    int steps = 60;

    for (int i = 0; i < steps; i++) {
        double theta = (double)i / steps * 3.14159;

        for (int j = 0; j < steps; j++) {
            double phi = (double)j / steps * 6.28318;

            double x = sphereR * sin(theta) * cos(phi);
            double y = sphereR * sin(theta) * sin(phi);
            double z = sphereR * cos(theta);

            rotate3D(x, y, z);

            double f = 300.0 / (z + 4.0);
            int sx = (int)(x * f + cx);
            int sy = (int)(y * f + cy);

            if (sx >= 0 && sx < w && sy >= 0 && sy < h)
                pixels[sy * w + sx] = 0xFFFFFF; // alb
        }
    }

    // -------------------------
    // TORUS COMPLET DIN PUNCTE
    // -------------------------
    double R = 2.0;   // raza mare
    double r = 0.7;   // raza mică

    for (int i = 0; i < steps; i++) {
        double u = (double)i / steps * 6.28318;

        for (int j = 0; j < steps; j++) {
            double v = (double)j / steps * 6.28318;

            double x = (R + r * cos(v)) * cos(u);
            double y = (R + r * cos(v)) * sin(u);
            double z = r * sin(v);

            rotate3D(x, y, z);

            double f = 300.0 / (z + 6.0);
            int sx = (int)(x * f + cx);
            int sy = (int)(y * f + cy);

            if (sx >= 0 && sx < w && sy >= 0 && sy < h)
                pixels[sy * w + sx] = 0x00FFFF; // cyan
        }
    }

    return vData{ 1LL };
};





}