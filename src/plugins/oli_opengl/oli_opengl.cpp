#include "../../OliEngine.hpp"
//#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>




#ifdef _WIN32
    #include <windows.h>
    #define OLI_EXPORT extern "C" __declspec(dllexport)
#else
    #define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#endif

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

// Tipurile tale
using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

struct GLState {
    int   width  = 0;
    int   height = 0;
    HWND  hwnd   = nullptr;
    HDC   hdc    = nullptr;
    HGLRC hrc    = nullptr;
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
/*
inline double toDouble(const vData& v) {
    if (std::holds_alternative<double>(v.value))     return std::get<double>(v.value);
    if (std::holds_alternative<long long>(v.value))  return (double)std::get<long long>(v.value);
    return 0.0;
}
*/

inline double toDouble(const vData& v) {
    return v.toDouble(); // Folosește metoda din vData.hpp care are deja getTrueData() inclus!
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
    // Notă: glUniform1f ar putea fi glUniform1fARB în funcție de vechimea plăcii
    glUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");

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
            wc.lpfnWndProc   = DefWindowProcW;
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
        glViewport(0, 0, w, h);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        glOrtho(0, w, h, 0, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        return vData{1LL};
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

        glClear(GL_COLOR_BUFFER_BIT);
        return vData{ 1LL };
    };

    // GL_LINE(x1, y1, x2, y2, color)
    registry[L"GL_LINE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 5) return vData{0LL};

        GLColor col((unsigned int)toDouble(args[4]));
        glColor3f(col.r, col.g, col.b);

        glBegin(GL_LINES);
            glVertex2f((float)toDouble(args[0]), (float)toDouble(args[1]));
            glVertex2f((float)toDouble(args[2]), (float)toDouble(args[3]));
        glEnd();

        return vData{1LL};
    };

    // GL_NODE(x, y, size, color)
    registry[L"GL_NODE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{0LL};

        float x   = (float)toDouble(args[0]);
        float y   = (float)toDouble(args[1]);
        float sz  = (float)toDouble(args[2]);
        GLColor c((unsigned int)toDouble(args[3]));

        glColor3f(c.r, c.g, c.b);
        glPointSize(sz);

        glBegin(GL_POINTS);
            glVertex2f(x, y);
        glEnd();

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

    // GL_POINT(x, y, size, color)
    registry[L"GL_POINT"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{0LL};

        float x   = (float)toDouble(args[0]);
        float y   = (float)toDouble(args[1]);
        float sz  = (float)toDouble(args[2]);
        GLColor c((unsigned int)toDouble(args[3]));

        glColor3f(c.r, c.g, c.b);
        glPointSize(sz);

        glBegin(GL_POINTS);
            glVertex2f(x, y);
        glEnd();

        return vData{1LL};
    };

    // GL_CIRCLE(x, y, radius, color)
    registry[L"GL_CIRCLE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{0LL};

        float cx  = (float)toDouble(args[0]);
        float cy  = (float)toDouble(args[1]);
        float r   = (float)toDouble(args[2]);
        GLColor c((unsigned int)toDouble(args[3]));

        glColor3f(c.r, c.g, c.b);

        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 64; ++i) {
            float a = i * 0.09817477f; // 2π / 64
            glVertex2f(cx + r * std::cos(a), cy + r * std::sin(a));
        }
        glEnd();

        return vData{1LL};
    };

    // GL_FILL_CIRCLE(x, y, radius, color)
    registry[L"GL_FILL_CIRCLE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{0LL};

        float cx  = (float)toDouble(args[0]);
        float cy  = (float)toDouble(args[1]);
        float r   = (float)toDouble(args[2]);
        GLColor c((unsigned int)toDouble(args[3]));

        glColor3f(c.r, c.g, c.b);

        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx, cy);
            for (int i = 0; i <= 64; ++i) {
                float a = i * 0.09817477f;
                glVertex2f(cx + r * std::cos(a), cy + r * std::sin(a));
            }
        glEnd();

        return vData{1LL};
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
        return vData{1LL};
    };

    // GL_LOAD_SHADER(type, source)
    // type: 0 = vertex, 1 = fragment
    registry[L"GL_LOAD_SHADER"] = [](const std::vector<vData>& args) -> vData {
        //printf("[INFO] in GL_LOAD_SHADER\n");
        // 1. Verificare argumente (trebuie să avem Tip și Sursă)
        if (args.size() < 2) {
            //printf("[ERROR] GL_LOAD_SHADER: Lipsesc argumente (necesar: tip, sursa)\n");
            return vData{ 0LL };
        }

        // 2. Verificăm dacă sursa este validă (prevenim crash la interpretare/bytecode)
        if (!args[1].isString()) {
           // printf("[ERROR] GL_LOAD_SHADER: Argumentul 2 (sursa) nu este String! Index tip detectat: %zu\n",
               // args[1].type());
            return vData{ 0LL };
        }

        // 3. Extragere și conversie
        int shaderTypeIndex = (int)args[0].toDouble(); // 0 = Vertex, 1 = Fragment
        std::wstring wsrc = args[1].toWString();

        if (wsrc.empty()) {
           // printf("[WARNING] GL_LOAD_SHADER: Sursa shaderului este goala!\n");
            return vData{ 0LL };
        }

        // Convertim wstring în string (folosește funcția ta wstr_to_str)
        std::string src = wstr_to_str(wsrc);

       // std::cout << "[DEBUG] Adresa string: " << (void*)src.data() << " Lungime: " << src.length() << std::endl;

        // 4. Secțiunea OpenGL (Verificăm dacă pointerii sunt încărcați)
        if (glCreateShader == nullptr || glShaderSource == nullptr || glCompileShader == nullptr) {
          //  printf("[CRITICAL ERROR] Functiile OpenGL moderne nu sunt incarcate! (wglGetProcAddress failed)\n");
            return vData{ 0LL };
        }

        // Decidem tipul de shader
        GLenum glType = (shaderTypeIndex == 0) ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
        GLuint shader = glCreateShader(glType);

        const char* csrc = src.c_str();
        glShaderSource(shader, 1, &csrc, NULL);
        glCompileShader(shader);

        // 5. Verificare Compilare (Diagnostic crucial pentru Bytecode)
        GLint success = 0;
        if (glGetShaderiv != nullptr) {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                char infoLog[512];
                if (glGetShaderInfoLog != nullptr) {
                    glGetShaderInfoLog(shader, 512, NULL, infoLog);
                   // printf("[OPENGL ERROR] Eroare compilare shader:\n%s\n", infoLog);

                    // Opțional: Printăm sursa primită ca să vedem dacă e "mutilată"
                    // printf("Sursa primita a fost:\n%s\n", src.c_str());
                }
            }
            else {
               // printf("[SUCCESS] Shader compilat cu succes. ID: %u\n", shader);
            }
        }

        return vData{ (long long)shader };
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
        glBegin(GL_TRIANGLES);
            glVertex2f(-1.0f, -1.0f);
            glVertex2f( 3.0f, -1.0f);
            glVertex2f(-1.0f,  3.0f);
        glEnd();
        return vData{1LL};
    };

    // GL_SET_UNIFORM(program, name, value)
    /*
    registry[L"GL_SET_UNIFORM"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{0LL};

        GLuint program   = (GLuint)toDouble(args[0]);
        std::string name = wstr_to_str(args[1].toWString());
        float value      = (float)toDouble(args[2]);

        GLint loc = glGetUniformLocation(program, name.c_str());
        if (loc >= 0)
            glUniform1f(loc, value);

        return vData{1LL};
    };
    */
    // În proiectul plugin-ului tău (oli_opengl.cpp)
    registry[L"GL_SET_UNIFORM"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 3) return vData{ 0LL };

        GLuint program = (GLuint)toDouble(args[0]);
        std::string name = wstr_to_str(args[1].toWString());
        GLint loc = glGetUniformLocation(program, name.c_str());

        if (loc == -1) return vData{ 0LL };

        // Dacă avem 4 argumente: Program, Nume, X, Y -> vec2
        if (args.size() == 4) {
            if (glUniform2f) {
                glUniform2f(loc, (float)toDouble(args[2]), (float)toDouble(args[3]));
            }
        }
        // Dacă avem 3 argumente: Program, Nume, Valoare -> float
        else if (args.size() == 3) {
            if (glUniform1f) {
                glUniform1f(loc, (float)toDouble(args[2]));
            }
        }

        return vData{ 1LL };
        };
}


