include ../Makefile.plugin.common

# Flag-uri de optimizare și incluziuni globale
CXXFLAGS += -I../../ -O3 -fPIC

ifdef IS_WINDOWS
    # Pe Windows legăm contextul nativ Win32 și subsistemul OpenGL Microsoft
    LDFLAGS += -lopengl32 -lgdi32 -lglu32 -luser32
else
    # Pe Linux legăm serverul de ferestre X11, nucleul OpenGL (Mesa) și librăria de utilitare (GLU)
    LDFLAGS += -lGL -lGLU -lX11 -lm
endif
