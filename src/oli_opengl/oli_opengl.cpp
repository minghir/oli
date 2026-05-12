#include "../../OliEngine.hpp"
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
    for (wchar_t wc : wstr) {
        if (wc <= 0x7F) {
            str.push_back(static_cast<char>(wc));
        } else {
            // Conversie manuală pentru caractere non-ASCII (poate fi îmbunătățită)
            str.push_back('?'); // Înlocuiește caracterele non-ASCII cu '?'
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

inline double toDouble(const vData& v) {
    if (std::holds_alternative<double>(v.value))     return std::get<double>(v.value);
    if (std::holds_alternative<long long>(v.value))  return (double)std::get<long long>(v.value);
    return 0.0;
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
        if (!args.empty()) {
            unsigned int hex = (unsigned int)toDouble(args[0]);
            GLColor c(hex);
            glClearColor(c.r, c.g, c.b, 1.0f);
        } else {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        }

        glClear(GL_COLOR_BUFFER_BIT);
        return vData{1LL};
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
        SwapBuffers(g_GL.hdc);

        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return vData{1LL};
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
        if (args.size() < 2) return vData{0LL};

        int type = (int)toDouble(args[0]);
        std::string src = wstr_to_str(args[1].toWString());

        GLuint shader = glCreateShader(type == 0 ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
        const char* csrc = src.c_str();
        glShaderSource(shader, 1, &csrc, NULL);
        glCompileShader(shader);

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
}


/*
#include "../../OliEngine.hpp"
#include "../../StringUtils.hpp"
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



PFNGLCREATESHADERPROC glCreateShader = nullptr;
PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
PFNGLATTACHSHADERPROC glAttachShader = nullptr;
PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
PFNGLUSEPROGRAMPROC glUseProgram = nullptr;


PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
PFNGLUNIFORM1FPROC glUniform1f = nullptr;

// Folosim tipurile tale de date
using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

struct GLState {
    int width = 0;
    int height = 0;
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC hrc = nullptr; // Contextul de randare OpenGL
} g_GL;

// Helper pentru conversie culori (OpenGL folosește 0.0 - 1.0)
struct GLColor {
    float r, g, b;
    GLColor(unsigned int hex) {
        r = ((hex >> 16) & 0xFF) / 255.0f;
        g = ((hex >> 8) & 0xFF) / 255.0f;
        b = (hex & 0xFF) / 255.0f;
    }
};

inline double toDouble(const vData& v) {
    if (std::holds_alternative<double>(v.value)) return std::get<double>(v.value);
    if (std::holds_alternative<long long>(v.value)) return static_cast<double>(std::get<long long>(v.value));
    return 0.0;
}

// Punctul de intrare pe care îl caută motorul
OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {

	
	registry[L"GL_INIT"] = [](const std::vector<vData>& args) -> vData {
    if (args.size() < 2) return vData{0LL};

    int w = (int)toDouble(args[0]);
    int h = (int)toDouble(args[1]);
    std::wstring title = (args.size() >= 3) ? args[2].toWString() : L"Oli OpenGL Window";

    g_GL.width = w;
    g_GL.height = h;

    HINSTANCE hInst = GetModuleHandle(NULL);

    // 1. Înregistrăm clasa ferestrei
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSW wc = {0};
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = hInst;
        wc.lpszClassName = L"OliGLClass";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        RegisterClassW(&wc);
        classRegistered = true;
    }

    // 2. Dimensiune corectă pentru client area
    RECT rc = {0, 0, w, h};
    DWORD dwStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    AdjustWindowRect(&rc, dwStyle, FALSE);

    // 3. Creăm fereastra
    g_GL.hwnd = CreateWindowExW(
        0, L"OliGLClass", title.c_str(),
        dwStyle, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInst, NULL
    );

    if (!g_GL.hwnd) return vData{0LL};

    // 4. Device context
    g_GL.hdc = GetDC(g_GL.hwnd);

    // 5. Pixel format
    PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1 };
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;

    int format = ChoosePixelFormat(g_GL.hdc, &pfd);
    if (!format) return vData{0LL};

    SetPixelFormat(g_GL.hdc, format, &pfd);

    // 6. Creăm contextul OpenGL
    g_GL.hrc = wglCreateContext(g_GL.hdc);
    if (!g_GL.hrc) return vData{0LL};

    wglMakeCurrent(g_GL.hdc, g_GL.hrc);

    // 7. Încărcăm funcțiile moderne OpenGL (OBLIGATORIU după wglMakeCurrent)
    glCreateShader       = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
    glShaderSource       = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
    glCompileShader      = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
    glCreateProgram      = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
    glAttachShader       = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
    glLinkProgram        = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
    glUseProgram         = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
    glUniform1f          = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
	
			// 7. Setup OpenGL pentru shadere
		glViewport(0, 0, w, h);

		// Dezactivăm complet pipeline-ul fixed function
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		// IMPORTANT: fără glOrtho, fără transformări fixe

		// Activăm blending (pentru transparență)
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Dezactivăm depth test (nu e nevoie la fullscreen)
		glDisable(GL_DEPTH_TEST);

		// Dezactivăm culling (unele GPU-uri nu desenează triunghiul fullscreen)
		glDisable(GL_CULL_FACE);
	



    // IMPORTANT: fără glOrtho, fără transformări fixe
    // Shaderul va primi gl_FragCoord corect

    return vData{1LL};
};

	
	
	registry[L"GL_CLEAR"] = [](const std::vector<vData>& args) -> vData {
		// Dacă primim un argument, îl folosim ca culoare de fundal (format hex 0xRRGGBB)
		if (!args.empty()) {
			unsigned int hex = static_cast<unsigned int>(toDouble(args[0]));
			float r = ((hex >> 16) & 0xFF) / 255.0f;
			float g = ((hex >> 8) & 0xFF) / 255.0f;
			float b = (hex & 0xFF) / 255.0f;
			glClearColor(r, g, b, 1.0f);
		} else {
			// Default: Negru
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		}

		// Curățăm buffer-ul de culoare și cel de adâncime
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		return vData{ 1LL }; // Returnăm succes
	};
	
	// GL_LINE(x1, y1, x2, y2, color) - Ideal pentru sinapse
    registry[L"GL_LINE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 5) return vData{ 0LL };
        
        GLColor col(static_cast<unsigned int>(toDouble(args[4])));
        glColor3f(col.r, col.g, col.b);

        glBegin(GL_LINES);
            glVertex2f((float)toDouble(args[0]), (float)toDouble(args[1]));
            glVertex2f((float)toDouble(args[2]), (float)toDouble(args[3]));
        glEnd();
        
        return vData{ 1LL };
    };

    // GL_NODE(x, y, size, color) - Pentru neuroni
    registry[L"GL_NODE"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return vData{ 0LL };
        
        float x = (float)toDouble(args[0]);
        float y = (float)toDouble(args[1]);
        float size = (float)toDouble(args[2]);
        GLColor col(static_cast<unsigned int>(toDouble(args[3])));

        glColor3f(col.r, col.g, col.b);
        glPointSize(size);
        glBegin(GL_POINTS);
            glVertex2f(x, y);
        glEnd();
        
        return vData{ 1LL };
    };

    // GL_PRESENT - Inlocuieste BitBlt cu SwapBuffers
    registry[L"GL_PRESENT"] = [](const std::vector<vData>&) -> vData {
        SwapBuffers(g_GL.hdc);
        
        // Pompa de mesaje Windows
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

		float x = (float)toDouble(args[0]);
		float y = (float)toDouble(args[1]);
		float size = (float)toDouble(args[2]);
		GLColor col((unsigned int)toDouble(args[3]));

		glColor3f(col.r, col.g, col.b);
		glPointSize(size);

		glBegin(GL_POINTS);
			glVertex2f(x, y);
		glEnd();

		return vData{1LL};
	};

	// GL_CIRCLE(x, y, radius, color)
	registry[L"GL_CIRCLE"] = [](const std::vector<vData>& args) -> vData {
		if (args.size() < 4) return vData{0LL};

		float cx = (float)toDouble(args[0]);
		float cy = (float)toDouble(args[1]);
		float r  = (float)toDouble(args[2]);
		GLColor col((unsigned int)toDouble(args[3]));

		glColor3f(col.r, col.g, col.b);

		glBegin(GL_LINE_LOOP);
		for (int i = 0; i < 64; i++) {
			float a = i * 0.09817477f; // 2π / 64
			glVertex2f(cx + r * std::cos(a), cy + r * std::sin(a));
		}
		glEnd();

		return vData{1LL};
	};

	// GL_FILL_CIRCLE(x, y, radius, color)
	registry[L"GL_FILL_CIRCLE"] = [](const std::vector<vData>& args) -> vData {
		if (args.size() < 4) return vData{0LL};

		float cx = (float)toDouble(args[0]);
		float cy = (float)toDouble(args[1]);
		float r  = (float)toDouble(args[2]);
		GLColor col((unsigned int)toDouble(args[3]));

		glColor3f(col.r, col.g, col.b);

		glBegin(GL_TRIANGLE_FAN);
			glVertex2f(cx, cy);
			for (int i = 0; i <= 64; i++) {
				float a = i * 0.09817477f;
				glVertex2f(cx + r * std::cos(a), cy + r * std::sin(a));
			}
		glEnd();

		return vData{1LL};
	};

	
	registry[L"GL_CLOSE"] = [](const std::vector<vData>& args) -> vData {
#ifdef _WIN32
    if (g_GL.hrc) {
        // 1. Dezactivăm contextul curent
        wglMakeCurrent(NULL, NULL);
        // 2. Ștergem contextul de randare OpenGL
        wglDeleteContext(g_GL.hrc);
        g_GL.hrc = nullptr;
    }

    if (g_GL.hwnd) {
        // 3. Eliberăm Device Context-ul
        if (g_GL.hdc) {
            ReleaseDC(g_GL.hwnd, g_GL.hdc);
            g_GL.hdc = nullptr;
        }
        // 4. Distrugem fereastra fizică
        DestroyWindow(g_GL.hwnd);
        g_GL.hwnd = nullptr;
    }
#else
    // Logica pentru Linux/X11 dacă este cazul
#endif

		return vData{ 1LL }; // Succes
	};
	
	registry[L"GL_LOAD_SHADER"] = [](const std::vector<vData>& args) -> vData {
		if (args.size() < 2) return vData{0LL};

		int type = (int)toDouble(args[0]); // 0 = vertex, 1 = fragment
		std::string src = wstr_to_str(args[1].toWString());

		GLuint shader = glCreateShader(type == 0 ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);

		const char* csrc = src.c_str();
		glShaderSource(shader, 1, &csrc, NULL);
		glCompileShader(shader);

		return vData{ (long long)shader };
	};
	
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
	
	registry[L"GL_USE_PROGRAM"] = [](const std::vector<vData>& args) -> vData {
		if (args.size() < 1) return vData{0LL};

		GLuint program = (GLuint)toDouble(args[0]);
		glUseProgram(program);

		return vData{1LL};
	};
	
	registry[L"GL_DRAW_FULLSCREEN"] = [](const std::vector<vData>& args) -> vData {
		glBegin(GL_TRIANGLES);
			glVertex2f(-1, -1);
			glVertex2f( 3, -1);
			glVertex2f(-1,  3);
		glEnd();
		return vData{1LL};
	};
	
	registry[L"GL_SET_UNIFORM"] = [](const std::vector<vData>& args) -> vData {
		if (args.size() < 3) return vData{0LL};

		GLuint program = (GLuint)toDouble(args[0]);
		std::string name = wstr_to_str(args[1].toWString());
		float value = (float)toDouble(args[2]);

		GLint loc = glGetUniformLocation(program, name.c_str());
		if (loc >= 0)
			glUniform1f(loc, value);

		return vData{1LL};
	};


	
}
*/