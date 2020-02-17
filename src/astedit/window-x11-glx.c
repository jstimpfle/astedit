#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <astedit/x11.h>
#include <astedit/window.h>
#include <X11/Xlib.h>
//#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>

static const GLint att[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB, 1,
        GLX_SAMPLE_BUFFERS, 1, // <-- MSAA
        GLX_SAMPLES, 4, // <-- MSAA
        None,
};

static XVisualInfo *visualInfo;
static Colormap colormap;
static GLXContext contextGlx;

static int initialWindowWidth = 800;
static int initialWindowHeight = 800;

ANY_FUNCTION *window_get_OpenGL_function_pointer(const char *name)
{
        return glXGetProcAddress((const GLubyte *) name);
}

void swap_buffers(void)
{
        glXSwapBuffers(display, window);
}

void setup_window(void)
{
        display = XOpenDisplay(NULL);
        if (display == NULL)
                fatalf("Failed to XOpenDisplay()");

        screen = DefaultScreen(display);
        rootWin = DefaultRootWindow(display);

        // FBConfigs were added in GLX version 1.3.
        int glx_major, glx_minor;
        if (!glXQueryVersion(display, &glx_major, &glx_minor) ||
             (glx_major == 1 && glx_minor < 3) || (glx_major < 1))
                fatalf("Invalid GLX version");

        GLXFBConfig bestFBC;
        {
                int fbcount;
                GLXFBConfig *fbc = glXChooseFBConfig(display, screen, att, &fbcount);
                if (fbc == NULL || fbcount == 0)
                        fatalf("Failed to retrieve a framebuffer config");
                bestFBC = fbc[0]; // TODO: really choose best
                XFree(fbc);
        }
        visualInfo = glXGetVisualFromFBConfig(display, bestFBC);

        colormap = XCreateColormap(display, rootWin,
                                   visualInfo->visual, AllocNone);

        XSetWindowAttributes wa = {0};
        wa.colormap = colormap;
        wa.event_mask = ExposureMask
                | KeyPressMask | KeyReleaseMask
                | ButtonPressMask | ButtonReleaseMask
                | PointerMotionMask;

        window = XCreateWindow(display, rootWin, 0, 0,
                               initialWindowWidth, initialWindowHeight,
                               0, visualInfo->depth,
                               InputOutput, visualInfo->visual,
                               CWColormap | CWEventMask, &wa);

        XMapWindow(display, window);
        XStoreName(display, window, "Untitled Window");

        // NOTE: It is not necessary to create or make current to a context before
        // calling glXGetProcAddressARB
        PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB = 0;
        glXCreateContextAttribsARB = (PFNGLXCREATECONTEXTATTRIBSARBPROC) glXGetProcAddressARB((const GLubyte *) "glXCreateContextAttribsARB");

        {
                int context_attribs[] =
                {
                        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
                        GLX_CONTEXT_MINOR_VERSION_ARB, 2,
                        //GLX_CONTEXT_FLAGS_ARB        , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
                        None
                };

                contextGlx = glXCreateContextAttribsARB( display, bestFBC, 0, True, context_attribs );
        }

        glXMakeCurrent(display, window, contextGlx);
}

void teardown_window(void)
{
        glXMakeCurrent(display, None, NULL);
        glXDestroyContext(display, contextGlx);
        XDestroyWindow(display, window);
        XFreeColormap(display, colormap);
        XCloseDisplay(display);
}
