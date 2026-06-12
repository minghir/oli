#include "../../OliEngine.hpp"
#include <GL/gl.h>
#include <GL/glu.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <map>

// Dynamic Loading pentru extensii OpenGL moderne (Shadere/GLSL)
#ifdef _WIN32
    #include <windows.h>
    #include <GL/glext.h>
    #define OLI_EXPORT extern "C" __declspec(dllexport)
    #define GET_GL_PROC(name, type) name = (type)wglGetProcAddress(#name);
#else
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <GL/glx.h>
    #include <GL/glxext.h>
    #define OLI_EXPORT extern "C" __attribute__((visibility("default")))
    #define GET_GL_PROC(name, type) name = (type)glXGetProcAddress((const GLubyte*)#name);
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// -----------------------------------------------------------------------------
// STRUCTURI DATE MODELE & CORPURI 3D
// -----------------------------------------------------------------------------
struct Vertex { float x, y, z; };
struct Normal { float x, y, z; };
struct FaceVertex { int vIdx; int nIdx; };
struct Material { float r, g, b; };
struct Face { std::vector<FaceVertex> points; std::string matName; };
struct Model {
    std::vector<Vertex> vertices;
    std::vector<Normal> normals;
    std::vector<Face> faces;
    std::map<std::string, Material> materials;
};

std::unordered_map<int, Model> g_Models;
int g_NextModelID = 1;

std::unordered_map<int, GLuint> g_Textures;
int g_NextTexID = 1;

std::string g_LastError = "";

// -----------------------------------------------------------------------------
// CONTEXT OPERATING SYSTEM STATE (CROSS-PLATFORM)
// -----------------------------------------------------------------------------
struct GLState {
    int   width  = 0;
    int   height = 0;
    int   mouseX = 0;
    int   mouseY = 0;
    bool  buttons[3] = { false, false, false }; // Left, Right, Middle
    GLuint fontBase = 0;

#ifdef _WIN32
    HWND  hwnd   = nullptr;
    HDC   hdc    = nullptr;
    HGLRC hrc    = nullptr;
#else
    Display* display = nullptr;
    Window      window  = 0;
    GLXContext  context = nullptr;
    Atom        wmDeleteMessage;
#endif
} g_GL;

struct GLColor {
    float r, g, b;
    GLColor(unsigned int hex) {
        r = ((hex >> 16) & 0xFF) / 255.0f;
        g = ((hex >> 8)  & 0xFF) / 255.0f;
        b = ( hex        & 0xFF) / 255.0f;
    }
};

// -----------------------------------------------------------------------------
// POINTERI SHADERE OPENGL (GLSL)
// -----------------------------------------------------------------------------
PFNGLCREATESHADERPROC       glCreateShader       = nullptr;
PFNGLSHADERSOURCEPROC       glShaderSource       = nullptr;
PFNGLCOMPILESHADERPROC      glCompileShader      = nullptr;
PFNGLCREATEPROGRAMPROC      glCreateProgram      = nullptr;
PFNGLATTACHSHADERPROC       glAttachShader       = nullptr;
PFNGLLINKPROGRAMPROC        glLinkProgram        = nullptr;
PFNGLUSEPROGRAMPROC         glUseProgram         = nullptr;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
PFNGLUNIFORM1FPROC          glUniform1f          = nullptr;
PFNGLGETSHADERIVPROC        glGetShaderiv        = nullptr;
PFNGLGETSHADERINFOLOGPROC   glGetShaderInfoLog   = nullptr;
PFNGLGETPROGRAMIVPROC       glGetProgramiv       = nullptr;
PFNGLGETPROGRAMINFOLOGPROC  glGetProgramInfoLog  = nullptr;
PFNGLUNIFORM2FPROC          glUniform2f          = nullptr;
PFNGLUNIFORM4FPROC          glUniform4f          = nullptr;
PFNGLDELETESHADERPROC       glDeleteShader       = nullptr;

#ifdef _WIN32
typedef bool (WINAPI* PFNWGLSWAPINTERVALEXTPROC)(int interval);
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;
#else
//typedef int (*PFNGLXSWAPINTERVALEXTPROC)(Display* dpy, GLXDrawable drawable, int interval);
//PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = nullptr;
void (*glXSwapIntervalEXT)(Display*, GLXDrawable, int) = nullptr;
#endif

// -----------------------------------------------------------------------------
// UTILITARE CONVERSIE STRING
// -----------------------------------------------------------------------------
std::string wstr_to_str(const std::wstring& wstr) {
    std::string str;
    for (size_t i = 0; i < wstr.length(); ++i) {
        if (wstr[i] == L'\\' && i + 1 < wstr.length() && wstr[i + 1] == L'n') {
            str.push_back('\n'); i++; continue;
        }
        str.push_back(wstr[i] <= 0x7F ? static_cast<char>(wstr[i]) : ' ');
    }
    return str;
}

std::wstring str_to_wstr(const std::string& str) {
    return std::wstring(str.begin(), str.end());
}

inline double toDouble(const vData& v) { return v.toDouble(); }

// -----------------------------------------------------------------------------
// ÎNCĂRCARE ETICHETE TEXT (CROSS-PLATFORM BITMAP FONTS)
// -----------------------------------------------------------------------------
void BuildFont() {
    g_GL.fontBase = glGenLists(96);
#ifdef _WIN32
    HFONT font = CreateFontA(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Arial");
    SelectObject(g_GL.hdc, font);
    wglUseFontBitmapsA(g_GL.hdc, 32, 96, g_GL.fontBase);
#else
    // Forțăm o deschidere safe. Nu folosim direct XLoadFont, care crapă la BadName,
    // ci încercăm să stângem informații despre structura fontului în mod securizat.
    XFontStruct* fontInfo = XLoadQueryFont(g_GL.display, "-*-fixed-bold-r-normal--18-*-*-*-*-*-iso8859-1");
    
    if (!fontInfo) {
        // Fallback 1: Încercăm o dimensiune mai comună de fixed bold
        fontInfo = XLoadQueryFont(g_GL.display, "-*-fixed-medium-r-normal--16-*-*-*-*-*-iso8859-1");
    }
    
    if (!fontInfo) {
        // Fallback 2: Încercăm fontul universal prezent în absolut orice distribuție Linux existentă
        fontInfo = XLoadQueryFont(g_GL.display, "fixed");
    }

    if (fontInfo) {
        // Generăm bitmap-urile OpenGL pe baza structurii de font încărcate cu succes
        glXUseXFont(fontInfo->fid, 32, 96, g_GL.fontBase);
        XFreeFont(g_GL.display, fontInfo); // Eliberăm structura din memoria X11, acum e în GPU
    } else {
        std::cerr << "[OpenGL Plugin] AVERTISMENT CRITIC: Nu s-a putut încărca niciun font bitmap X11!" << std::endl;
    }
#endif
}

// -----------------------------------------------------------------------------
// PARSERE ȘI ÎNCĂRCĂTOARE TEXTURI / MODELE 3D
// -----------------------------------------------------------------------------
std::map<std::string, Material> LoadMTL(const std::string& path) {
    std::map<std::string, Material> materials;
    std::ifstream file(path);
    if (!file.is_open()) return materials;
    std::string line, currentMat;
    while (std::getline(file, line)) {
        std::stringstream ss(line); std::string prefix; ss >> prefix;
        if (prefix == "newmtl") { ss >> currentMat; }
        else if (prefix == "Kd") { Material m; ss >> m.r >> m.g >> m.b; materials[currentMat] = m; }
    }
    return materials;
}

int LoadOBJ(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return 0;
    Model model; std::string line; std::string activeMaterial = ""; std::string folder = "";
    size_t lastSlash = path.find_last_of("\\/");
    if (lastSlash != std::string::npos) folder = path.substr(0, lastSlash + 1);

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line); std::string prefix; ss >> prefix;
        if (prefix == "mtllib") { std::string mtlFileName; ss >> mtlFileName; model.materials = LoadMTL(folder + mtlFileName); }
        else if (prefix == "usemtl") { ss >> activeMaterial; }
        else if (prefix == "v") { Vertex v; ss >> v.x >> v.y >> v.z; model.vertices.push_back(v); }
        else if (prefix == "vn") { Normal n; ss >> n.x >> n.y >> n.z; model.normals.push_back(n); }
        else if (prefix == "f") {
            Face face; face.matName = activeMaterial; std::string segment;
            while (ss >> segment) {
                FaceVertex fv = { -1, -1 };
                try {
                    size_t firstSlash = segment.find('/'); size_t lastSlash = segment.rfind('/');
                    fv.vIdx = std::stoi(segment.substr(0, firstSlash)) - 1;
                    if (firstSlash != std::string::npos && lastSlash != firstSlash) {
                        fv.nIdx = std::stoi(segment.substr(lastSlash + 1)) - 1;
                    }
                } catch (...) { continue; }
                face.points.push_back(fv);
            }
            if (!face.points.empty()) model.faces.push_back(face);
        }
    }
    if (model.vertices.empty()) return 0;
    int id = g_NextModelID++; g_Models[id] = model;
    return id;
}

int LoadTexture(const std::string& path) {
    int width, height, nrChannels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
    if (!data) return 0;
    GLuint texture; glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    int id = g_NextTexID++; g_Textures[id] = texture;
    return id;
}

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
    
    // 🔥 ASIGURĂ-TE CĂ ACESTEA SUNT ÎNCĂRCATE PENTRU VEC2 ȘI VEC4:
    GET_GL_PROC(glUniform2f, PFNGLUNIFORM2FPROC);
    GET_GL_PROC(glUniform4f, PFNGLUNIFORM4FPROC);
    GET_GL_PROC(glDeleteShader, PFNGLDELETESHADERPROC);
    
#ifdef _WIN32
    glUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
#else
    // 🔥 CRITIC PENTRU LINUX: Încărcăm glUniform1f (folosit de iTime!)
    GET_GL_PROC(glUniform1f, PFNGLUNIFORM1FPROC);
    glXSwapIntervalEXT = (void (*)(Display*, GLXDrawable, int))glXGetProcAddress((const GLubyte*)"glXSwapIntervalEXT");
#endif
}

// Windows Window Callback Procedure
#ifdef _WIN32
LRESULT CALLBACK OliWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_MOUSEMOVE: g_GL.mouseX = LOWORD(lParam); g_GL.mouseY = HIWORD(lParam); return 0;
    case WM_LBUTTONDOWN: g_GL.buttons[0] = true;  return 0;
    case WM_LBUTTONUP:   g_GL.buttons[0] = false; return 0;
    case WM_RBUTTONDOWN: g_GL.buttons[1] = true;  return 0;
    case WM_RBUTTONUP:   g_GL.buttons[1] = false; return 0;
    case WM_SIZE: {
        int newW = LOWORD(lParam); int newH = HIWORD(lParam);
        g_GL.width = newW; g_GL.height = newH;
        if (g_GL.hrc && g_GL.hdc) {
            wglMakeCurrent(g_GL.hdc, g_GL.hrc); glViewport(0, 0, newW, newH);
            glMatrixMode(GL_PROJECTION); glLoadIdentity();
            if (newH > 0) gluPerspective(45.0f, (float)newW / (float)newH, 0.1f, 1000.0f);
            glMatrixMode(GL_MODELVIEW);
        }
        return 0;
    }
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
#endif

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

#ifndef G_LINKED_OLI_ENGINE_DEFINED
#define G_LINKED_OLI_ENGINE_DEFINED
inline vOliEngine* g_LinkedOliEngine = nullptr;
#endif

// -----------------------------------------------------------------------------
// PUNCTUL DE INTRARE ÎN REGISTRUL PLUGINULUI OLI
// -----------------------------------------------------------------------------
//OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {
OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry, void* enginePtr) {

    if (g_LinkedOliEngine == nullptr) {
        g_LinkedOliEngine = static_cast<vOliEngine*>(enginePtr);
    }
    // GL_INIT(width, height, title)
    registry[L"GL_INIT"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{0LL};
        int w = (int)toDouble(args[0]);
        int h = (int)toDouble(args[1]);
        std::wstring title = (args.size() >= 3) ? args[2].toWString() : L"Oli OpenGL Native Window";
        std::string sTitle = wstr_to_str(title);

        g_GL.width  = w;
        g_GL.height = h;

#ifdef _WIN32
        HINSTANCE hInst = GetModuleHandle(NULL);
        static bool classRegistered = false;
        if (!classRegistered) {
            WNDCLASSW wc = {0}; wc.lpfnWndProc = OliWndProc; wc.hInstance = hInst;
            wc.lpszClassName = L"OliGLClass"; wc.hCursor = LoadCursor(NULL, IDC_ARROW);
            wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
            RegisterClassW(&wc); classRegistered = true;
        }
        RECT rc = {0, 0, w, h}; DWORD dwStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        AdjustWindowRect(&rc, dwStyle, FALSE);
        g_GL.hwnd = CreateWindowExW(0, L"OliGLClass", title.c_str(), dwStyle, CW_USEDEFAULT, CW_USEDEFAULT,
                                    rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInst, NULL);
        g_GL.hdc = GetDC(g_GL.hwnd);
        PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1 };
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cDepthBits = 24;
        int format = ChoosePixelFormat(g_GL.hdc, &pfd);
        SetPixelFormat(g_GL.hdc, format, &pfd);
        g_GL.hrc = wglCreateContext(g_GL.hdc);
        wglMakeCurrent(g_GL.hdc, g_GL.hrc);
#else
        g_GL.display = XOpenDisplay(nullptr);
        if (!g_GL.display) return vData{0LL};
        Window root = DefaultRootWindow(g_GL.display);
        GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
        XVisualInfo* vi = glXChooseVisual(g_GL.display, 0, att);
        XSetWindowAttributes swa;
        swa.colormap = XCreateColormap(g_GL.display, root, vi->visual, AllocNone);
        swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;
        g_GL.window = XCreateWindow(g_GL.display, root, 0, 0, w, h, 0, vi->depth,
                                    InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
        XMapWindow(g_GL.display, g_GL.window);
        XStoreName(g_GL.display, g_GL.window, sTitle.c_str());
        g_GL.context = glXCreateContext(g_GL.display, vi, nullptr, GL_TRUE);
        glXMakeCurrent(g_GL.display, g_GL.window, g_GL.context);
        g_GL.wmDeleteMessage = XInternAtom(g_GL.display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(g_GL.display, g_GL.window, &g_GL.wmDeleteMessage, 1);
#endif

        LoadShaderFunctions();
#ifdef _WIN32
        if (wglSwapIntervalEXT) wglSwapIntervalEXT(0);
#else
        if (glXSwapIntervalEXT) glXSwapIntervalEXT(g_GL.display, g_GL.window, 0);
#endif

        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        gluPerspective(45.0f, (float)w / (float)h, 0.1f, 1000.0f);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();

        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
        glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_COLOR_MATERIAL);

        float lightPos[] = { 5.0f, 5.0f, 5.0f, 1.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

        BuildFont();
        return vData{1LL};
    };

    // GL_PRESENT()
    registry[L"GL_PRESENT"] = [](const std::vector<vData>&) -> vData {
#ifdef _WIN32
        if (g_GL.hdc) SwapBuffers(g_GL.hdc);
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
#else
        if (g_GL.display && g_GL.window) glXSwapBuffers(g_GL.display, g_GL.window);
        while (XPending(g_GL.display)) {
            XEvent xev; XNextEvent(g_GL.display, &xev);
            if (xev.type == MotionNotify) {
                g_GL.mouseX = xev.xmotion.x; g_GL.mouseY = xev.xmotion.y;
            }
            else if (xev.type == ButtonPress) {
                if (xev.xbutton.button == Button1) g_GL.buttons[0] = true;
                if (xev.xbutton.button == Button3) g_GL.buttons[1] = true;
            }
            else if (xev.type == ButtonRelease) {
                if (xev.xbutton.button == Button1) g_GL.buttons[0] = false;
                if (xev.xbutton.button == Button3) g_GL.buttons[1] = false;
            }
            else if (xev.type == ConfigureNotify) {
                int newW = xev.xconfigure.width; int newH = xev.xconfigure.height;
                if (newW != g_GL.width || newH != g_GL.height) {
                    g_GL.width = newW; g_GL.height = newH;
                    glViewport(0, 0, newW, newH);
                    glMatrixMode(GL_PROJECTION); glLoadIdentity();
                    if (newH > 0) gluPerspective(45.0f, (float)newW / (float)newH, 0.1f, 1000.0f);
                    glMatrixMode(GL_MODELVIEW);
                }
            }
            else if (xev.type == ClientMessage) {
                if ((Atom)xev.xclient.data.l[0] == g_GL.wmDeleteMessage) {
                    fflush(stdout); return vData{0LL}; // Returnează 0 în script ca flag de oprire loop
                }
            }
        }
#endif
        return vData{ 1LL };
    };

    // GL_CLEAR(colorHex)
    registry[L"GL_CLEAR"] = [](const std::vector<vData>& args) -> vData {
#ifdef _WIN32
        if (g_GL.hdc && g_GL.hrc) wglMakeCurrent(g_GL.hdc, g_GL.hrc);
#else
        if (g_GL.display && g_GL.window && g_GL.context) glXMakeCurrent(g_GL.display, g_GL.window, g_GL.context);
#endif
        if (!args.empty()) {
            GLColor c((unsigned int)args[0].toDouble());
            glClearColor(c.r, c.g, c.b, 1.0f);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return vData{ 1LL };
    };

    // GL_LINE(x1, y1, x2, y2, color)
    registry[L"GL_LINE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 5) return vData{ 0LL };
        float x1 = (float)toDouble(args[0]); float y1 = (float)toDouble(args[1]);
        float x2 = (float)toDouble(args[2]); float y2 = (float)toDouble(args[3]);
        GLColor col((unsigned int)toDouble(args[4]));

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
        if (glUseProgram) glUseProgram(0);

        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
        glOrtho(0, g_GL.width, g_GL.height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

        glColor3f(col.r, col.g, col.b);
        glBegin(GL_LINES); glVertex2f(x1, y1); glVertex2f(x2, y2); glEnd();

        glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
        glMatrixMode(GL_MODELVIEW); glPopAttrib();
        return vData{ 1LL };
    };

    // GL_POINT(x, y, size, color)
    registry[L"GL_POINT"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{ 0LL };
        float x = (float)toDouble(args[0]); float y = (float)toDouble(args[1]);
        float sz = (float)toDouble(args[2]); GLColor c((unsigned int)toDouble(args[3]));

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);

        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
        glOrtho(0, g_GL.width, g_GL.height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

        glColor3f(c.r, c.g, c.b); glPointSize(sz);
        glBegin(GL_POINTS); glVertex2f(x, y); glEnd();

        glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
        glMatrixMode(GL_MODELVIEW); glPopAttrib();
        return vData{ 1LL };
    };

    // GL_CIRCLE(x, y, radius, color)
    registry[L"GL_CIRCLE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{ 0LL };
        float cx = (float)toDouble(args[0]); float cy = (float)toDouble(args[1]);
        float r = (float)toDouble(args[2]); GLColor c((unsigned int)toDouble(args[3]));

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
        if (glUseProgram) glUseProgram(0);

        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
        glOrtho(0, g_GL.width, g_GL.height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

        glColor3f(c.r, c.g, c.b); glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 64; ++i) {
            float a = i * 0.09817477f; glVertex2f(cx + r * std::cos(a), cy + r * std::sin(a));
        }
        glEnd();

        glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
        glMatrixMode(GL_MODELVIEW); glPopAttrib();
        return vData{ 1LL };
    };

    // GL_CLOSE()
    registry[L"GL_CLOSE"] = [](const std::vector<vData>&) -> vData {
#ifdef _WIN32
        if (g_GL.hrc) { wglMakeCurrent(NULL, NULL); wglDeleteContext(g_GL.hrc); g_GL.hrc = nullptr; }
        if (g_GL.hwnd) {
            if (g_GL.hdc) { ReleaseDC(g_GL.hwnd, g_GL.hdc); g_GL.hdc = nullptr; }
            DestroyWindow(g_GL.hwnd); g_GL.hwnd = nullptr;
        }
        MSG msg; while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
#else
        if (g_GL.display && g_GL.context) {
            glXMakeCurrent(g_GL.display, None, nullptr);
            glXDestroyContext(g_GL.display, g_GL.context);
            g_GL.context = nullptr;
        }
        if (g_GL.display && g_GL.window) { XDestroyWindow(g_GL.display, g_GL.window); g_GL.window = 0; }
        if (g_GL.display) { XCloseDisplay(g_GL.display); g_GL.display = nullptr; }
#endif
        fflush(stdout);
        return vData{1LL};
    };

    // GL_LOAD_SHADER(type, source)
    registry[L"GL_LOAD_SHADER"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2 || !args[1].isString()) return vData{ 0LL };
        int shaderTypeIndex = (int)toDouble(args[0]);
        std::string src = wstr_to_str(args[1].toWString());

        if (glCreateShader == nullptr) { g_LastError = "Critical: GLSL functions missing!"; return vData{ 0LL }; }

        GLenum glType = (shaderTypeIndex == 0) ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
        GLuint shader = glCreateShader(glType);
        const char* csrc = src.c_str();
        glShaderSource(shader, 1, &csrc, NULL);
        glCompileShader(shader);

        GLint success = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            GLint logLength = 0; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength > 1) {
                std::vector<GLchar> infoLog(logLength);
                glGetShaderInfoLog(shader, logLength, NULL, infoLog.data());
                g_LastError = std::string(infoLog.begin(), infoLog.end());
            }
            if (glDeleteShader) glDeleteShader(shader);
            return vData{ 0LL };
        }
        g_LastError = "No error";
        return vData{ (long long)shader };
    };

    registry[L"GL_GET_ERROR"] = [](const std::vector<vData>&) -> vData { return vData{ str_to_wstr(g_LastError) }; };

    // GL_LINK_PROGRAM(vs, fs)
    registry[L"GL_LINK_PROGRAM"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{0LL};
        GLuint vs = (GLuint)toDouble(args[0]); GLuint fs = (GLuint)toDouble(args[1]);
        GLuint program = glCreateProgram();
        glAttachShader(program, vs); glAttachShader(program, fs); glLinkProgram(program);
        return vData{ (long long)program };
    };

    // GL_USE_PROGRAM(program)
    registry[L"GL_USE_PROGRAM"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData{0LL};
        glUseProgram((GLuint)toDouble(args[0]));
        return vData{1LL};
    };

    // GL_DRAW_FULLSCREEN()
    registry[L"GL_DRAW_FULLSCREEN"] = [](const std::vector<vData>&) -> vData {
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING);
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

        glBegin(GL_TRIANGLES);
            glVertex2f(-1.0f, -1.0f); glVertex2f( 3.0f, -1.0f); glVertex2f(-1.0f,  3.0f);
        glEnd();

        glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW); glPopMatrix();
        glPopAttrib();
        return vData{1LL};
    };

    // GL_SET_UNIFORM(program, name, values...)
    registry[L"GL_SET_UNIFORM"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ 0LL };
        GLuint program = (GLuint)toDouble(args[0]);
        std::string name = wstr_to_str(args[1].toWString());
        GLint loc = glGetUniformLocation(program, name.c_str());
        if (loc == -1) return vData{ 0LL };

        if (args.size() == 6) {
            if (glUniform4f) glUniform4f(loc, (float)toDouble(args[2]), (float)toDouble(args[3]), (float)toDouble(args[4]), (float)toDouble(args[5]));
        } else if (args.size() == 4) {
            if (glUniform2f) glUniform2f(loc, (float)toDouble(args[2]), (float)toDouble(args[3]));
        } else if (args.size() == 3) {
            if (glUniform1f) glUniform1f(loc, (float)toDouble(args[2]));
        }
        return vData{ 1LL };
    };

    registry[L"GL_WIDTH"]  = [](const std::vector<vData>&) -> vData { return vData{ (long long)g_GL.width }; };
    registry[L"GL_HEIGHT"] = [](const std::vector<vData>&) -> vData { return vData{ (long long)g_GL.height }; };

    // GL_TEXT(x, y, string)
    registry[L"GL_TEXT"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ 0LL };
        float x = (float)toDouble(args[0]); float y = (float)toDouble(args[1]);
        std::string text = wstr_to_str(args[2].toWString());

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
        glOrtho(0, g_GL.width, g_GL.height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

        glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
        if (glUseProgram) glUseProgram(0);

        glColor3f(1.0f, 1.0f, 1.0f); glRasterPos2f(x, y);

        if (g_GL.fontBase > 0) {
            glListBase(g_GL.fontBase - 32);
            glCallLists((GLsizei)text.length(), GL_UNSIGNED_BYTE, text.c_str());
        }

        glMatrixMode(GL_MODELVIEW); glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
        glPopAttrib();
        return vData{ 1LL };
    };

    // GL_LOAD_MODEL(path)
    registry[L"GL_LOAD_MODEL"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData{ 0LL };
        std::string path = wstr_to_str(args[0].toWString());
        return vData{ (long long)LoadOBJ(path) };
    };

    // GL_DRAW_MODEL(id, x, y, z, rx, ry, rz, scale)
    registry[L"GL_DRAW_MODEL"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 8) return vData{ 0LL };
        int id = (int)toDouble(args[0]);
        if (g_Models.find(id) == g_Models.end()) return vData{ 0LL };

        float tx = (float)toDouble(args[1]); float ty = (float)toDouble(args[2]); float tz = (float)toDouble(args[3]);
        float rx = (float)toDouble(args[4]); float ry = (float)toDouble(args[5]); float rz = (float)toDouble(args[6]);
        float scale = (float)toDouble(args[7]);

        const Model& m = g_Models[id];
        glPushMatrix(); glLoadIdentity();
        glTranslatef(tx, ty, tz);
        glRotatef(rx, 1.0f, 0.0f, 0.0f); glRotatef(ry, 0.0f, 1.0f, 0.0f); glRotatef(rz, 0.0f, 0.0f, 1.0f);
        glScalef(scale, scale, scale);
        glEnable(GL_NORMALIZE);

        std::string lastMat = "";
        for (const auto& face : m.faces) {
            if (face.matName != lastMat) {
                auto it = m.materials.find(face.matName);
                if (it != m.materials.end()) {
                    const Material& mat = it->second;
                    float diffuse[] = { mat.r, mat.g, mat.b, 1.0f };
                    glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
                    glColor3f(mat.r, mat.g, mat.b);
                }
                lastMat = face.matName;
            }
            glBegin(GL_POLYGON);
            for (const auto& fv : face.points) {
                if (fv.nIdx >= 0 && fv.nIdx < (int)m.normals.size()) {
                    const Normal& n = m.normals[fv.nIdx]; glNormal3f(n.x, n.y, n.z);
                }
                if (fv.vIdx >= 0 && fv.vIdx < (int)m.vertices.size()) {
                    const Vertex& v = m.vertices[fv.vIdx]; glVertex3f(v.x, v.y, v.z);
                }
            }
            glEnd();
        }
        glDisable(GL_NORMALIZE); glPopMatrix();
        return vData{ 1LL };
    };

    // GL_LOAD_TEXTURE(path)
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

        float x = (float)toDouble(args[1]); float y = (float)toDouble(args[2]);
        float w = (float)toDouble(args[3]); float h = (float)toDouble(args[4]);
        float rot = (float)toDouble(args[5]);

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, g_Textures[id]);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);

        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
        glOrtho(-g_GL.width / 2, g_GL.width / 2, -g_GL.height / 2, g_GL.height / 2, -1, 1);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

        glTranslatef(x, y, 0); glRotatef(rot, 0, 0, 1); glColor4f(1, 1, 1, 1);

        glBegin(GL_QUADS);
            glTexCoord2f(0, 1); glVertex2f(-w / 2, -h / 2);
            glTexCoord2f(1, 1); glVertex2f(w / 2, -h / 2);
            glTexCoord2f(1, 0); glVertex2f(w / 2, h / 2);
            glTexCoord2f(0, 0); glVertex2f(-w / 2, h / 2);
        glEnd();

        glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
        glMatrixMode(GL_MODELVIEW); glPopAttrib();
        return vData{ 1LL };
    };

    registry[L"GL_NODE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{ 0LL };

        float x = (float)toDouble(args[0]);
        float y = (float)toDouble(args[1]);
        float sz = (float)toDouble(args[2]);
        GLColor c((unsigned int)toDouble(args[3]));

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
        if (glUseProgram) glUseProgram(0);

        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
        glOrtho(0, g_GL.width, g_GL.height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

        glColor3f(c.r, c.g, c.b); glPointSize(sz);
        glBegin(GL_POINTS); glVertex2f(x, y); glEnd();

        glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
        glMatrixMode(GL_MODELVIEW); glPopAttrib();

        return vData{ 1LL };
    };

    // 🔥 FIX: Înregistrare explicită GL_FILL_CIRCLE
    registry[L"GL_FILL_CIRCLE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{ 0LL };

        float cx = (float)toDouble(args[0]);
        float cy = (float)toDouble(args[1]);
        float r = (float)toDouble(args[2]);
        GLColor c((unsigned int)toDouble(args[3]));

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
        if (glUseProgram) glUseProgram(0);

        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
        glOrtho(0, g_GL.width, g_GL.height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

        glColor3f(c.r, c.g, c.b);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy); // Centrul cercului
        for (int i = 0; i <= 64; ++i) {
            float a = i * 0.09817477f; // 2π / 64
            glVertex2f(cx + r * std::cos(a), cy + r * std::sin(a));
        }
        glEnd();

        glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
        glMatrixMode(GL_MODELVIEW); glPopAttrib();

        return vData{ 1LL };
    };

    // 🔥 FIX: Înregistrare explicită GL_SET_VSYNC (Cross-Platform)
    // 🔥 VARIANTĂ ANTIGLONȚ: GL_SET_VSYNC (Nu mai permite crash-ul silențios)
    registry[L"GL_SET_VSYNC"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData{ 0LL };
        int interval = (int)toDouble(args[0]);

#ifdef _WIN32
        if (wglSwapIntervalEXT) {
            wglSwapIntervalEXT(interval);
            return vData{ 1LL };
        }
#else
        // Pe Linux, verificăm cu atenție pointerul și contextul înainte de a executa
        if (glXSwapIntervalEXT != nullptr && g_GL.display != nullptr && g_GL.window != 0) {
            try {
                // Înconjurăm apelul cu o verificare de siguranță a contextului curent
                GLXContext currentCtx = glXGetCurrentContext();
                if (currentCtx != nullptr) {
                    glXSwapIntervalEXT(g_GL.display, g_GL.window, interval);
                    return vData{ 1LL };
                }
            } catch (...) {
                std::cerr << "[OpenGL] VSync a eșuat la nivel de driver, dar am prevenit crash-ul!" << std::endl;
            }
        } else {
            // FALLBACK SILENȚIOS: Dacă extensia nu e stabilă sau lipsește,
            // doar ignorăm apelul (mai bine fără VSync decât cu programul închis)
            static bool warned = false;
            if (!warned) {
                std::cout << "[OpenGL] Memento: glXSwapIntervalEXT nu este complet compatibil cu driverul Mesa curent. Ignorăm apelul de siguranță." << std::endl;
                warned = true;
            }
        }
#endif
        return vData{ 0LL };
    };

    // GL_MOUSE_X / Y / BTN
    registry[L"GL_MOUSE_X"]   = [](const std::vector<vData>&) -> vData { return vData{ (long long)g_GL.mouseX }; };
    registry[L"GL_MOUSE_Y"]   = [](const std::vector<vData>&) -> vData { return vData{ (long long)g_GL.mouseY }; };
    registry[L"GL_MOUSE_BTN"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData{ 0LL };
        int btn = (int)toDouble(args[0]);
        if (btn >= 0 && btn < 3) return vData{ g_GL.buttons[btn] ? 1LL : 0LL };
        return vData{ 0LL };
    };
}