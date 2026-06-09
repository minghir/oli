#include "../../OliEngine.hpp"
//#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef _WIN32
    #include <windows.h>
    #define OLI_EXPORT extern "C" __declspec(dllexport)
#else
    #define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#endif

struct Vertex {
    float x, y, z;
};

struct Normal {
    float x, y, z;
};

struct FaceVertex {
    int vIdx; // Index Vertex
    int nIdx; // Index Normală
};

struct Material {
    float r, g, b;
};

struct Face {
    std::vector<FaceVertex> points;
    std::string matName;
};

struct Model {
    std::vector<Vertex> vertices;
    std::vector<Normal> normals;
    std::vector<Face> faces;
    std::map<std::string, Material> materials;
};

// Mapă globală pentru a stoca modelele încărcate (ID -> Model)
std::unordered_map<int, Model> g_Models;
int g_NextModelID = 1;


std::map<std::string, Material> LoadMTL(const std::string& path) {
    std::map<std::string, Material> materials;
    std::ifstream file(path);
    if (!file.is_open()) return materials;

    std::string line, currentMat;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;

        if (prefix == "newmtl") {
            ss >> currentMat;
        }
        else if (prefix == "Kd") {
            Material m;
            ss >> m.r >> m.g >> m.b;
            materials[currentMat] = m;
        }
    }
    return materials;
}


int LoadOBJ(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return 0;
    }

    Model model;
    std::string line;
    std::string activeMaterial = ""; // Reține materialul curent setat prin 'usemtl'

    // Determinăm directorul în care se află fișierul .obj pentru a căuta .mtl în același loc
    std::string folder = "";
    size_t lastSlash = path.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        folder = path.substr(0, lastSlash + 1);
    }

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;

        // 0. MATERIAL LIBRARY - Încarcă definițiile de culori
        if (prefix == "mtllib") {
            std::string mtlFileName;
            ss >> mtlFileName;
            // Încărcăm fișierul .mtl folosind calea completă
            model.materials = LoadMTL(folder + mtlFileName);
        }
        // 1. USE MATERIAL - Schimbă materialul activ pentru următoarele fețe
        else if (prefix == "usemtl") {
            ss >> activeMaterial;
        }
        // 2. VÂRFURI
        else if (prefix == "v") {
            Vertex v;
            ss >> v.x >> v.y >> v.z;
            model.vertices.push_back(v);
        }
        // 3. NORMALE
        else if (prefix == "vn") {
            Normal n;
            ss >> n.x >> n.y >> n.z;
            model.normals.push_back(n);
        }
        // 4. FEȚE
        else if (prefix == "f") {
            Face face;
            face.matName = activeMaterial; // Atribuim materialul activ acestei fețe
            
            std::string segment;
            while (ss >> segment) {
                FaceVertex fv = { -1, -1 };

                try {
                    size_t firstSlash = segment.find('/');
                    size_t lastSlash = segment.rfind('/');

                    // Index Vertex
                    fv.vIdx = std::stoi(segment.substr(0, firstSlash)) - 1;

                    // Index Normală (v/t/n sau v//n)
                    if (firstSlash != std::string::npos && lastSlash != firstSlash) {
                        fv.nIdx = std::stoi(segment.substr(lastSlash + 1)) - 1;
                    }
                }
                catch (...) {
                    continue;
                }
                face.points.push_back(fv);
            }

            if (!face.points.empty()) {
                model.faces.push_back(face);
            }
        }
    }

    if (model.vertices.empty()) return 0;

    int id = g_NextModelID++;
    g_Models[id] = model;

    return id;
}


std::string wstr_to_str(const std::wstring& wstr) {
    std::string str;
    for (size_t i = 0; i < wstr.length(); ++i) {
        // Fix pentru \n binar
        if (wstr[i] == L'\\' && i + 1 < wstr.length() && wstr[i + 1] == L'n') {
            str.push_back('\n');
            i++; // Sarim peste 'n'
            continue;
        }

        if (wstr[i] <= 0x7F) {
            str.push_back(static_cast<char>(wstr[i]));
        }
        else {
            str.push_back(' '); // Mai sigur decat '?'
        }
    }
    return str;
}


// Mapă globală pentru texturi (ID -> OpenGL Texture Handle)
std::unordered_map<int, GLuint> g_Textures;
int g_NextTexID = 1;

// Helper pentru încărcare textură
int LoadTexture(const std::string& path) {
    int width, height, nrChannels;
    // Forțăm încărcarea cu 4 canale (RGBA) pentru a suporta transparența
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);

    if (!data) return 0;

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Parametri de filtrare (Pixel-art style sau Smooth)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    // Încărcăm datele în GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);

    int id = g_NextTexID++;
    g_Textures[id] = texture;
    return id;
}


typedef bool (WINAPI* PFNWGLSWAPINTERVALEXTPROC)(int interval);
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;


// Funcții OpenGL moderne (GLSL)
PFNGLCREATESHADERPROC      glCreateShader       = nullptr;
PFNGLSHADERSOURCEPROC      glShaderSource       = nullptr;
PFNGLCOMPILESHADERPROC     glCompileShader      = nullptr;
PFNGLCREATEPROGRAMPROC     glCreateProgram      = nullptr;
PFNGLATTACHSHADERPROC      glAttachShader       = nullptr;
PFNGLLINKPROGRAMPROC       glLinkProgram        = nullptr;
PFNGLUSEPROGRAMPROC        glUseProgram         = nullptr;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
PFNGLUNIFORM1FPROC         glUniform1f          = nullptr;
PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;

// Dacă ai nevoie și de erori de program (Link):
PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;

PFNGLUNIFORM2FPROC glUniform2f = nullptr;
PFNGLUNIFORM4FPROC glUniform4f = nullptr;

PFNGLDELETESHADERPROC glDeleteShader = nullptr;

// Tipurile tale
using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

struct GLState {
    int   width  = 0;
    int   height = 0;
    HWND  hwnd   = nullptr;
    HDC   hdc    = nullptr;
    HGLRC hrc    = nullptr;
    GLuint fontBase = 0;
    int mouseX = 0;
    int mouseY = 0;
    bool buttons[3] = { false, false, false }; // Stânga, Dreapta, Mijloc
} g_GL;

// Helper culoare
struct GLColor {
    float r, g, b;
    GLColor(unsigned int hex) {
        r = ((hex >> 16) & 0xFF) / 255.0f;
        g = ((hex >> 8)  & 0xFF) / 255.0f;
        b = ( hex        & 0xFF) / 255.0f;
    }
};


std::string g_LastError = ""; // Aici vom stoca ultima eroare GLSL

// Helper pentru conversie înapoi la wstring (necesar pentru GL_GET_ERROR)
std::wstring str_to_wstr(const std::string& str) {
    return std::wstring(str.begin(), str.end());
}


// 1. Creăm fontul (apelează asta în GL_INIT)
void BuildFont() {
    g_GL.fontBase = glGenLists(96);
    HFONT font = CreateFontA(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Arial");
    SelectObject(g_GL.hdc, font);
    wglUseFontBitmapsA(g_GL.hdc, 32, 96, g_GL.fontBase);
}

inline double toDouble(const vData& v) {
    return v.toDouble(); // Folosește metoda din vData.hpp care are deja getTrueData() inclus!
}


LRESULT CALLBACK OliWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_MOUSEMOVE:
        g_GL.mouseX = LOWORD(lParam);
        g_GL.mouseY = HIWORD(lParam);
        return 0;

    case WM_LBUTTONDOWN: g_GL.buttons[0] = true;  return 0;
    case WM_LBUTTONUP:   g_GL.buttons[0] = false; return 0;
    case WM_RBUTTONDOWN: g_GL.buttons[1] = true;  return 0;
    case WM_RBUTTONUP:   g_GL.buttons[1] = false; return 0;
    case WM_SIZE: {
        int newW = LOWORD(lParam);
        int newH = HIWORD(lParam);

        g_GL.width = newW;
        g_GL.height = newH;

        if (g_GL.hrc && g_GL.hdc) {
            wglMakeCurrent(g_GL.hdc, g_GL.hrc);
            glViewport(0, 0, newW, newH);

            // RE-CALCULĂM PERSPECTIVA 3D AICI!
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            if (newH > 0) {
                gluPerspective(45.0f, (float)newW / (float)newH, 0.1f, 1000.0f);
            }
            glMatrixMode(GL_MODELVIEW);
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

#define GET_GL_PROC(name, type) name = (type)wglGetProcAddress(#name);

void LoadShaderFunctions() {
    GET_GL_PROC(glCreateShader, PFNGLCREATESHADERPROC);
    GET_GL_PROC(glShaderSource, PFNGLSHADERSOURCEPROC);
    GET_GL_PROC(glCompileShader, PFNGLCOMPILESHADERPROC);
    GET_GL_PROC(glCreateProgram, PFNGLCREATEPROGRAMPROC);
    GET_GL_PROC(glAttachShader, PFNGLATTACHSHADERPROC);
    GET_GL_PROC(glLinkProgram, PFNGLLINKPROGRAMPROC);
    GET_GL_PROC(glUseProgram, PFNGLUSEPROGRAMPROC);
    GET_GL_PROC(glGetUniformLocation, PFNGLGETUNIFORMLOCATIONPROC);
    GET_GL_PROC(glGetShaderiv, PFNGLGETSHADERIVPROC);
    GET_GL_PROC(glGetShaderInfoLog, PFNGLGETSHADERINFOLOGPROC);

    GET_GL_PROC(glGetProgramiv, PFNGLGETPROGRAMIVPROC);
    GET_GL_PROC(glGetProgramInfoLog, PFNGLGETPROGRAMINFOLOGPROC);
    GET_GL_PROC(glUniform2f, PFNGLUNIFORM2FPROC);
    GET_GL_PROC(glUniform4f, PFNGLUNIFORM4FPROC);
    GET_GL_PROC(glDeleteShader, PFNGLDELETESHADERPROC);
    // Notă: glUniform1f ar putea fi glUniform1fARB în funcție de vechimea plăcii
    glUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");

    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

}




// Punctul de intrare
OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {

    // GL_INIT(w, h, title)
    registry[L"GL_INIT"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{0LL};

        int w = (int)toDouble(args[0]);
        int h = (int)toDouble(args[1]);
        std::wstring title = (args.size() >= 3) ? args[2].toWString() : L"Oli OpenGL Window";

        g_GL.width  = w;
        g_GL.height = h;

        HINSTANCE hInst = GetModuleHandle(NULL);

        // 1. Clasa ferestrei
        static bool classRegistered = false;
        if (!classRegistered) {
            WNDCLASSW wc = {0};
            //wc.lpfnWndProc   = DefWindowProcW;
            wc.lpfnWndProc = OliWndProc;
            wc.hInstance     = hInst;
            wc.lpszClassName = L"OliGLClass";
            wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
            wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
            RegisterClassW(&wc);
            classRegistered = true;
        }

        // 2. Dimensiune client area
        RECT rc = {0, 0, w, h};
        DWORD dwStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        AdjustWindowRect(&rc, dwStyle, FALSE);

        // 3. Fereastra
        g_GL.hwnd = CreateWindowExW(
            0, L"OliGLClass", title.c_str(),
            dwStyle, CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top,
            NULL, NULL, hInst, NULL
        );
        if (!g_GL.hwnd) return vData{0LL};

       

        // 4. DC
        g_GL.hdc = GetDC(g_GL.hwnd);

        // 5. Pixel format
        PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1 };
        pfd.dwFlags   = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType= PFD_TYPE_RGBA;
        pfd.cColorBits= 32;
        pfd.cDepthBits= 24;

        int format = ChoosePixelFormat(g_GL.hdc, &pfd);
        if (!format) return vData{0LL};
        SetPixelFormat(g_GL.hdc, format, &pfd);

        // 6. Context OpenGL
        g_GL.hrc = wglCreateContext(g_GL.hdc);
        if (!g_GL.hrc) return vData{0LL};
        wglMakeCurrent(g_GL.hdc, g_GL.hrc);
        
        LoadShaderFunctions();
        if (wglSwapIntervalEXT) wglSwapIntervalEXT(0);

        // 7. Funcții moderne OpenGL (GLSL)
        glCreateShader       = (PFNGLCREATESHADERPROC)      wglGetProcAddress("glCreateShader");
        glShaderSource       = (PFNGLSHADERSOURCEPROC)      wglGetProcAddress("glShaderSource");
        glCompileShader      = (PFNGLCOMPILESHADERPROC)     wglGetProcAddress("glCompileShader");
        glCreateProgram      = (PFNGLCREATEPROGRAMPROC)     wglGetProcAddress("glCreateProgram");
        glAttachShader       = (PFNGLATTACHSHADERPROC)      wglGetProcAddress("glAttachShader");
        glLinkProgram        = (PFNGLLINKPROGRAMPROC)       wglGetProcAddress("glLinkProgram");
        glUseProgram         = (PFNGLUSEPROGRAMPROC)        wglGetProcAddress("glUseProgram");
        glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
        glUniform1f          = (PFNGLUNIFORM1FPROC)         wglGetProcAddress("glUniform1f");

        // 8. Setup pentru shadere (screen-space)
       // 8. Setup pentru 3D
		glViewport(0, 0, w, h);

		// Trecem pe Proiecție pentru a seta lentila camerei (Perspectiva)
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		// Setează perspectiva (FOV, Aspect Ratio, Near, Far)
		gluPerspective(45.0f, (float)w / (float)h, 0.1f, 1000.0f);

		// REVENIM pe ModelView pentru a desena obiectele
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDisable(GL_CULL_FACE);

		glEnable(GL_LIGHTING);   // Activăm sistemul de iluminare
		glEnable(GL_LIGHT0);     // Activăm prima sursă de lumină
		glEnable(GL_COLOR_MATERIAL); // Permite culorilor glColor să interacționeze cu lumina

		// Poziția luminii (deasupra și în lateral)
		float lightPos[] = { 5.0f, 5.0f, 5.0f, 1.0f };
		glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

		BuildFont();

        return vData{1LL};
    };

    // GL_PRESENT()
    registry[L"GL_PRESENT"] = [](const std::vector<vData>&) -> vData {
        if (g_GL.hdc) {
            SwapBuffers(g_GL.hdc);
        }

        // Pompa de mesaje (obligatorie ca Windows să nu creadă că aplicația e blocată)
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return vData{ 1LL };
        };

    // GL_CLEAR(colorHex)
    registry[L"GL_CLEAR"] = [](const std::vector<vData>& args) -> vData {
        // FORȚĂM contextul să fie curent pe acest thread
        if (g_GL.hdc && g_GL.hrc) {
            wglMakeCurrent(g_GL.hdc, g_GL.hrc);
        }

        if (!args.empty()) {
            // Folosim metoda toDouble() direct din obiect, e cea mai sigură
            unsigned int hex = (unsigned int)args[0].toDouble();

            // DEBUG HARDCODAT: Dacă vrei să fii 100% sigur, ignoră hex-ul o secundă:
            // glClearColor(1.0f, 0.0f, 0.0f, 1.0f); // Ar trebui să fie ROȘU

            GLColor c(hex);
            glClearColor(c.r, c.g, c.b, 1.0f);
        }

        //glClear(GL_COLOR_BUFFER_BIT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return vData{ 1LL };
    };

    // GL_LINE(x1, y1, x2, y2, color)
    // =================================================================
    // 🔥 GL_LINE (Corectat cu Izolator Screen-Space 2D)
    // =================================================================
    registry[L"GL_LINE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 5) return vData{ 0LL };

        float x1 = (float)toDouble(args[0]);
        float y1 = (float)toDouble(args[1]);
        float x2 = (float)toDouble(args[2]);
        float y2 = (float)toDouble(args[3]);
        GLColor col((unsigned int)toDouble(args[4]));

        // 1. Salvăm starea 3D curentă și dezactivăm lumina/texturile pentru linii pure 2D
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(0); // Ne asigurăm că nu este vreun shader 3D activ

        // 2. Comutăm proiecția pe mod Pixel-Perfect Ortho (0,0 sus-stânga)
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, g_GL.width, g_GL.height, 0, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        // 3. Desenăm linia nativ
        glColor3f(col.r, col.g, col.b);
        glBegin(GL_LINES);
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
        glEnd();

        // 4. Restaurăm complet starea anterioară pentru pipeline-ul 3D
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopAttrib();

        return vData{ 1LL };
        };

    // =================================================================
    // 🔥 GL_NODE (Corectat cu Izolator Screen-Space 2D)
    // =================================================================
    registry[L"GL_NODE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{ 0LL };

        float x = (float)toDouble(args[0]);
        float y = (float)toDouble(args[1]);
        float sz = (float)toDouble(args[2]);
        GLColor c((unsigned int)toDouble(args[3]));

        // 1. Salvăm starea 3D curentă
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(0);

        // 2. Configuram proiecția 2D
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, g_GL.width, g_GL.height, 0, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        // 3. Desenăm nodul punctiform
        glColor3f(c.r, c.g, c.b);
        glPointSize(sz);

        glBegin(GL_POINTS);
        glVertex2f(x, y);
        glEnd();

        // 4. Restaurăm starea 3D
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopAttrib();

        return vData{ 1LL };
        };

    

    // GL_POINT(x, y, size, color)
    /*
    registry[L"GL_POINT"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{ 0LL };

        float x = (float)toDouble(args[0]);
        float y = (float)toDouble(args[1]);
        float sz = (float)toDouble(args[2]);
        GLColor c((unsigned int)toDouble(args[3]));

        // 1. SALVĂM ȘI RESETĂM TOTUL
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);

        // 2. PROIECȚIA (Spațiul 2D)
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(-g_GL.width / 2.0, g_GL.width / 2.0, -g_GL.height / 2.0, g_GL.height / 2.0, -1, 1);

        // 3. MODELVIEW (Aici e buba!)
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity(); // <--- ACEASTA este linia care "taie firul" cu nava!

        // 4. DESENARE
        glColor3f(c.r, c.g, c.b);
        glPointSize(sz);
        glBegin(GL_POINTS);
        glVertex2f(x, y); // Acum x este ABSOLUT față de centrul ecranului
        glEnd();

        // 5. RESTAURARE
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW); // Revenim la modul standard
        glPopAttrib();

        return vData{ 1LL };
        };
        */

    registry[L"GL_POINT"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{ 0LL };

        float x = (float)toDouble(args[0]);
        float y = (float)toDouble(args[1]);
        float sz = (float)toDouble(args[2]);
        GLColor c((unsigned int)toDouble(args[3]));

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();

        // 🔥 CORECTURA AICI: 0,0 este acum sus-stânga!
        glOrtho(0, g_GL.width, g_GL.height, 0, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glColor3f(c.r, c.g, c.b);
        glPointSize(sz);
        glBegin(GL_POINTS);
        glVertex2f(x, y);
        glEnd();

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopAttrib();

        return vData{ 1LL };
        };

    // GL_CIRCLE(x, y, radius, color)
    // =================================================================
    // 🔥 GL_CIRCLE (Corectat cu Izolator Screen-Space 2D)
    // =================================================================
    registry[L"GL_CIRCLE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{ 0LL };

        float cx = (float)toDouble(args[0]);
        float cy = (float)toDouble(args[1]);
        float r = (float)toDouble(args[2]);
        GLColor c((unsigned int)toDouble(args[3]));

        // 1. Salvăm starea 3D curentă și dezactivăm lumina/programele
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(0);

        // 2. Comutăm pe mod Pixel-Perfect Ortho (0,0 sus-stânga)
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, g_GL.width, g_GL.height, 0, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        // 3. Desenăm cercul liniar în spațiul ecranului
        glColor3f(c.r, c.g, c.b);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 64; ++i) {
            float a = i * 0.09817477f; // 2π / 64
            glVertex2f(cx + r * std::cos(a), cy + r * std::sin(a));
        }
        glEnd();

        // 4. Restaurăm complet starea anterioară pentru pipeline-ul 3D
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopAttrib();

        return vData{ 1LL };
        };

    // =================================================================
    // 🔥 GL_FILL_CIRCLE (Corectat cu Izolator Screen-Space 2D)
    // =================================================================
    registry[L"GL_FILL_CIRCLE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{ 0LL };

        float cx = (float)toDouble(args[0]);
        float cy = (float)toDouble(args[1]);
        float r = (float)toDouble(args[2]);
        GLColor c((unsigned int)toDouble(args[3]));

        // 1. Salvăm starea 3D curentă
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(0);

        // 2. Comutăm pe mod Pixel-Perfect Ortho (0,0 sus-stânga)
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, g_GL.width, g_GL.height, 0, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        // 3. Desenăm discul plin (Triangle Fan)
        glColor3f(c.r, c.g, c.b);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= 64; ++i) {
            float a = i * 0.09817477f;
            glVertex2f(cx + r * std::cos(a), cy + r * std::sin(a));
        }
        glEnd();

        // 4. Restaurăm complet starea anterioară pentru pipeline-ul 3D
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopAttrib();

        return vData{ 1LL };
        };

    // GL_CLOSE()
    registry[L"GL_CLOSE"] = [](const std::vector<vData>&) -> vData {
    #ifdef _WIN32
        if (g_GL.hrc) {
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(g_GL.hrc);
            g_GL.hrc = nullptr;
        }

        if (g_GL.hwnd) {
            if (g_GL.hdc) {
                ReleaseDC(g_GL.hwnd, g_GL.hdc);
                g_GL.hdc = nullptr;
            }
            DestroyWindow(g_GL.hwnd);
            g_GL.hwnd = nullptr;
        }
    #endif

        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            DispatchMessageW(&msg);
        }

        // Dacă procesul principal nu se închide, poți forța un flush la stdout
        fflush(stdout);

        return vData{1LL};
    };
    
        // GL_LOAD_SHADER(type, source)
    // type: 0 = vertex, 1 = fragment
    registry[L"GL_LOAD_SHADER"] = [](const std::vector<vData>& args) -> vData {
        // 1. Verificare argumente (Tip și Sursă)
        if (args.size() < 2) return vData{ 0LL };

        // 2. Verificăm validitatea sursei
        if (!args[1].isString()) return vData{ 0LL };

        // 3. Extragere și conversie wstring -> string
        int shaderTypeIndex = (int)toDouble(args[0]);
        std::wstring wsrc = args[1].toWString();

        if (wsrc.empty()) return vData{ 0LL };

        std::string src = wstr_to_str(wsrc);

        // 4. Verificare Pointeri OpenGL (Prevenire Crash)
        if (glCreateShader == nullptr || glShaderSource == nullptr ||
            glCompileShader == nullptr || glGetShaderiv == nullptr ||
            glGetShaderInfoLog == nullptr) {
            g_LastError = "Critical: OpenGL shader functions not loaded!";
            return vData{ 0LL };
        }

        // 5. Creare și Compilare Shader
        GLenum glType = (shaderTypeIndex == 0) ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
        GLuint shader = glCreateShader(glType);

        const char* csrc = src.c_str();
        glShaderSource(shader, 1, &csrc, NULL);
        glCompileShader(shader);

        // 6. Verificare Compilare (Diagnostic)
        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

        if (!success) {
            GLint logLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

            if (logLength > 1) {
                // Folosim GLchar pentru vector conform standardului OpenGL
                std::vector<GLchar> infoLog(logLength);
                glGetShaderInfoLog(shader, logLength, NULL, infoLog.data());

                // Convertim log-ul în string-ul global pentru Oli
                g_LastError = std::string(infoLog.begin(), infoLog.end());

                // Output pentru debug
                std::string finalMsg = "\n--- GLSL COMPILE ERROR ---\n" + g_LastError + "--------------------------\n";
                //printf("%s", finalMsg.c_str());
                //OutputDebugStringA(finalMsg.c_str());
            }
            else {
                g_LastError = "Unknown GLSL error: Driver returned empty log.";
            }

            // Curățăm resursele dacă a eșuat
            if (glDeleteShader != nullptr) {
                glDeleteShader(shader);
            }

            return vData{ 0LL }; // Returnăm 0 în Oli pentru a semnala eșecul
        }

        // Totul e OK
        g_LastError = "No error";
        return vData{ (long long)shader };
        };

    registry[L"GL_GET_ERROR"] = [](const std::vector<vData>&) -> vData {
        // Folosim variabila globala si functia safe
        return vData{ str_to_wstr(g_LastError) };
        };

    // GL_LINK_PROGRAM(vs, fs)
    registry[L"GL_LINK_PROGRAM"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{0LL};

        GLuint vs = (GLuint)toDouble(args[0]);
        GLuint fs = (GLuint)toDouble(args[1]);

        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);

        return vData{ (long long)program };
    };

    // GL_USE_PROGRAM(program)
    registry[L"GL_USE_PROGRAM"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 1) return vData{0LL};

        GLuint program = (GLuint)toDouble(args[0]);
        glUseProgram(program);

        return vData{1LL};
    };

    // GL_DRAW_FULLSCREEN()
    registry[L"GL_DRAW_FULLSCREEN"] = [](const std::vector<vData>&) -> vData {
		// 1. Salvăm atributele actuale (ca să nu stricăm setările de la 3D)
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		
		// 2. Dezactivăm ce nu ne trebuie pentru un shader full-screen
		glDisable(GL_DEPTH_TEST); 
		glDisable(GL_LIGHTING);   
		
		// 3. Resetăm matricile la "Identity" (fără perspectivă, fără translație)
		// Asta face ca coordonatele -1...1 să fie fix marginile ecranului
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		// 4. Desenăm triunghiul imens care acoperă ecranul
		glBegin(GL_TRIANGLES);
			glVertex2f(-1.0f, -1.0f);
			glVertex2f( 3.0f, -1.0f);
			glVertex2f(-1.0f,  3.0f);
		glEnd();

		// 5. Restaurăm matricile și atributele anterioare (revenim la 3D-ul nostru)
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		
		glPopAttrib();

		return vData{1LL};
	};

    // În proiectul plugin-ului tău (oli_opengl.cpp)
    registry[L"GL_SET_UNIFORM"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ 0LL };

        GLuint program = (GLuint)toDouble(args[0]);
        std::string name = wstr_to_str(args[1].toWString());
        GLint loc = glGetUniformLocation(program, name.c_str());

        if (loc == -1) return vData{ 0LL };

        if (args.size() == 6) { // Program, Nume, X, Y, Z, W
            if (glUniform4f) glUniform4f(loc, (float)toDouble(args[2]), (float)toDouble(args[3]),
                (float)toDouble(args[4]), (float)toDouble(args[5]));
        }
        else if (args.size() == 4) { // vec2
            if (glUniform2f) glUniform2f(loc, (float)toDouble(args[2]), (float)toDouble(args[3]));
        }
        else if (args.size() == 3) { // float
            if (glUniform1f) glUniform1f(loc, (float)toDouble(args[2]));
        }
        return vData{ 1LL };
        };

    registry[L"GL_WIDTH"] = [](const std::vector<vData>& args) -> vData {
        return vData{ (long long)g_GL.width };
        };

    registry[L"GL_HEIGHT"] = [](const std::vector<vData>& args) -> vData {
        return vData{ (long long)g_GL.height };
        };

    // 2. În registry, adăugăm GL_TEXT(x, y, string)
    /*
    registry[L"GL_TEXT"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ 0LL };

        float x = (float)toDouble(args[0]);
        float y = (float)toDouble(args[1]);
        std::string text = wstr_to_str(args[2].toWString());

        // 1. SALVĂM TOATE MATRICELE ȘI STĂRILE
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();

        // SCHIMBARE: Folosim coordonate centrate ca la Sprite-uri
        // Asta permite folosirea valorilor negative pentru stânga/jos
        glOrtho(-g_GL.width / 2.0, g_GL.width / 2.0, -g_GL.height / 2.0, g_GL.height / 2.0, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        // 2. CONFIGURĂM STAREA
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        glUseProgram(0);

        glColor3f(1.0f, 1.0f, 1.0f); // Alb stralucitor
        glRasterPos2f(x, y);         // Acum x=0, y=0 e centrul ecranului!

        if (g_GL.fontBase > 0) {
            glListBase(g_GL.fontBase - 32);
            glCallLists((GLsizei)text.length(), GL_UNSIGNED_BYTE, text.c_str());
        }

        // 3. RESTAURĂM TOTUL EXACT CUM A FOST
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();

        glPopAttrib();

        return vData{ 1LL };
        };
        */

    registry[L"GL_TEXT"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ 0LL };

        float x = (float)toDouble(args[0]);
        float y = (float)toDouble(args[1]);
        std::string text = wstr_to_str(args[2].toWString());

        // 1. SALVĂM STAREA CURENTĂ
        glPushAttrib(GL_ALL_ATTRIB_BITS);

        // 2. SETĂM PROIECȚIA PENTRU 2D (SCREEN SPACE)
        glMatrixMode(GL_PROJECTION);
        glPushMatrix(); // Salvăm matricea 3D
        glLoadIdentity();
        // 0,0 în stânga-sus, width/height în dreapta-jos
        glOrtho(0, g_GL.width, g_GL.height, 0, -1, 1);

        // 3. SETĂM MODELVIEW (Unde desenăm)
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix(); // Salvăm poziția 3D
        glLoadIdentity();

        // 4. DESENARE
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        glUseProgram(0);

        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2f(x, y); // Acum (x,y) sunt coordonate reale de ecran

        if (g_GL.fontBase > 0) {
            glListBase(g_GL.fontBase - 32);
            glCallLists((GLsizei)text.length(), GL_UNSIGNED_BYTE, text.c_str());
        }

        // 5. RESTAURĂM TOTUL (Ordinea este inversă față de push!)
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();

        glPopAttrib();

        return vData{ 1LL };
        };

    registry[L"GL_SET_VSYNC"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData{ 0LL };

        int interval = (int)toDouble(args[0]); // 1 pentru ON, 0 pentru OFF

        if (wglSwapIntervalEXT) {
            wglSwapIntervalEXT(interval);
            return vData{ 1LL };
        }

        return vData{ 0LL }; // Returnăm 0 dacă extensia nu e suportată de placă
        };

    registry[L"GL_MOUSE_X"] = [](const std::vector<vData>&) -> vData {
        return vData{ (long long)g_GL.mouseX };
        };

    registry[L"GL_MOUSE_Y"] = [](const std::vector<vData>&) -> vData {
        return vData{ (long long)g_GL.mouseY };
        };

    registry[L"GL_MOUSE_BTN"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData{ 0LL };
        int btn = (int)toDouble(args[0]); // 0=stânga, 1=dreapta
        if (btn >= 0 && btn < 3) return vData{ g_GL.buttons[btn] ? 1LL : 0LL };
        return vData{ 0LL };
        };
		
		// În LoadOliPlugin:

	// GL_LOAD_MODEL(path) -> returnează ID-ul modelului
	registry[L"GL_LOAD_MODEL"] = [](const std::vector<vData>& args) -> vData {
		if (args.empty()) return vData{ 0LL };
		std::string path = wstr_to_str(args[0].toWString());
		return vData{ (long long)LoadOBJ(path) };
	};

	// GL_DRAW_MODEL(id, x, y, z, scale)
	registry[L"GL_DRAW_MODEL"] = [](const std::vector<vData>& args) -> vData {
		// Așteptăm 8 argumente: id, x, y, z, rx, ry, rz, scale
		if (args.size() < 8) return vData{ 0LL };
		
		int id = (int)toDouble(args[0]);
		if (g_Models.find(id) == g_Models.end()) return vData{ 0LL };

		float tx = (float)toDouble(args[1]);
		float ty = (float)toDouble(args[2]);
		float tz = (float)toDouble(args[3]);
		float rx = (float)toDouble(args[4]); 
		float ry = (float)toDouble(args[5]); 
		float rz = (float)toDouble(args[6]); 
		float scale = (float)toDouble(args[7]);

		const Model& m = g_Models[id];

		glPushMatrix();
        glLoadIdentity();
		// 1. Transformări de bază
		glTranslatef(tx, ty, tz);
		glRotatef(rx, 1.0f, 0.0f, 0.0f);
		glRotatef(ry, 0.0f, 1.0f, 0.0f);
		glRotatef(rz, 0.0f, 0.0f, 1.0f);
		glScalef(scale, scale, scale);

		glEnable(GL_NORMALIZE);

		// 2. Variabilă pentru optimizarea schimbării de materiale
		std::string lastMat = "";

		// 3. Iterăm prin fiecare față (care acum conține puncte și un nume de material)
		for (const auto& face : m.faces) {
			
			// Aplicăm materialul doar dacă s-a schimbat față de fața anterioară
			if (face.matName != lastMat) {
				auto it = m.materials.find(face.matName);
				if (it != m.materials.end()) {
					const Material& mat = it->second;
					float diffuse[] = { mat.r, mat.g, mat.b, 1.0f };
					
					// Setăm culoarea pentru sistemul de iluminare
					glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
					// Fallback în cazul în care iluminarea este oprită
					glColor3f(mat.r, mat.g, mat.b);
				}
				lastMat = face.matName;
			}

			glBegin(GL_POLYGON); 
			for (const auto& fv : face.points) {
				
				// SIGURANȚĂ 1: Verificăm indicii de normale
				if (fv.nIdx >= 0 && fv.nIdx < (int)m.normals.size()) {
					const Normal& n = m.normals[fv.nIdx];
					glNormal3f(n.x, n.y, n.z);
				}

				// SIGURANȚĂ 2: Verificăm indicii de vîrfuri
				if (fv.vIdx >= 0 && fv.vIdx < (int)m.vertices.size()) {
					const Vertex& v = m.vertices[fv.vIdx];
					glVertex3f(v.x, v.y, v.z);
				}
			}
			glEnd();
		}

		glDisable(GL_NORMALIZE);
		glPopMatrix();
		
		return vData{ 1LL };
	};

    // GL_LOAD_TEXTURE(path) -> returnează ID-ul texturii
    registry[L"GL_LOAD_TEXTURE"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData{ 0LL };
        std::string path = wstr_to_str(args[0].toWString());
        return vData{ (long long)LoadTexture(path) };
        };

    // GL_DRAW_SPRITE(id, x, y, w, h, rot)
    registry[L"GL_DRAW_SPRITE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 6) return vData{ 0LL };

        int id = (int)toDouble(args[0]);
        if (g_Textures.find(id) == g_Textures.end()) return vData{ 0LL };

        float x = (float)toDouble(args[1]);
        float y = (float)toDouble(args[2]);
        float w = (float)toDouble(args[3]);
        float h = (float)toDouble(args[4]);
        float rot = (float)toDouble(args[5]);

        // --- SETUP 2D OVER 3D ---
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, g_Textures[id]);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        // Folosim glOrtho pentru coordonate de ecran (ca în GL_TEXT)
        glOrtho(-g_GL.width / 2, g_GL.width / 2, -g_GL.height / 2, g_GL.height / 2, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        // --- DESENARE ---
        glTranslatef(x, y, 0);
        glRotatef(rot, 0, 0, 1);
        glColor4f(1, 1, 1, 1); // Culoare albă (păstrează culorile originale ale imaginii)

        glBegin(GL_QUADS);
        glTexCoord2f(0, 1); glVertex2f(-w / 2, -h / 2);
        glTexCoord2f(1, 1); glVertex2f(w / 2, -h / 2);
        glTexCoord2f(1, 0); glVertex2f(w / 2, h / 2);
        glTexCoord2f(0, 0); glVertex2f(-w / 2, h / 2);
        glEnd();

        // --- RESTORE ---
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopAttrib();

        return vData{ 1LL };
        };
}


