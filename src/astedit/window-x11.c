#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <astedit/window.h>
#include <astedit/clock.h>

#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h> // glXCreateContextAttribsARB

#include "x11keysym-to-unicode.h"

static const struct {
        int xkeysym;
        int keyKind;
} keymap[] = {
        { XK_Return, KEY_ENTER, },
        { XK_space, KEY_SPACE, },
        { XK_comma, KEY_NONE, },
        { XK_period, KEY_NONE, },
        { XK_colon, KEY_NONE, },
        { XK_semicolon, KEY_NONE, },
        { XK_Escape, KEY_ESCAPE },
        { XK_BackSpace, KEY_BACKSPACE },
        { XK_Left, KEY_CURSORLEFT },
        { XK_Right, KEY_CURSORRIGHT },
        { XK_Up, KEY_CURSORUP },
        { XK_Down, KEY_CURSORDOWN },
        { XK_a, KEY_A, },
        { XK_b, KEY_B, },
        { XK_c, KEY_C, },
        { XK_d, KEY_D, },
        { XK_e, KEY_E, },
        { XK_f, KEY_F, },
        { XK_g, KEY_G, },
        { XK_h, KEY_H, },
        { XK_i, KEY_I, },
        { XK_j, KEY_J, },
        { XK_k, KEY_K, },
        { XK_l, KEY_L, },
        { XK_m, KEY_M, },
        { XK_n, KEY_N, },
        { XK_o, KEY_O, },
        { XK_p, KEY_P, },
        { XK_q, KEY_Q, },
        { XK_r, KEY_R, },
        { XK_s, KEY_S, },
        { XK_t, KEY_T, },
        { XK_u, KEY_U, },
        { XK_w, KEY_W, },
        { XK_x, KEY_X, },
        { XK_y, KEY_Y, },
        { XK_z, KEY_Z, },
        { XK_0, KEY_0, },
        { XK_1, KEY_1, },
        { XK_2, KEY_2, },
        { XK_3, KEY_3, },
        { XK_4, KEY_4, },
        { XK_5, KEY_5, },
        { XK_6, KEY_6, },
        { XK_7, KEY_7, },
        { XK_8, KEY_8, },
        { XK_9, KEY_9, },
};

static void send_mousemove_event(int x, int y)
{
        struct Input input;
        input.inputKind = INPUT_CURSORMOVE;
        input.data.tCursormove.pixelX = x;
        input.data.tCursormove.pixelY = y;
        enqueue_input(&input);
}

static void send_mousebutton_event(int mousebuttonKind, int mousebuttoneventKind)
{
        struct Input input;
        input.inputKind = INPUT_MOUSEBUTTON;
        input.data.tMousebutton.mousebuttonKind = mousebuttonKind;
        input.data.tMousebutton.mousebuttonEventKind = mousebuttoneventKind;
        input.data.tMousebutton.modifiers = 0;
        enqueue_input(&input);
}

static void send_scroll_event(int keyKind)
{
        struct Input input;
        input.inputKind = INPUT_KEY;
        input.data.tKey.keyKind = keyKind;
        input.data.tKey.keyEventKind = KEYEVENT_PRESS;
        enqueue_input(&input);
}

static void send_key_event(int keyKind, int keyeventKind, int modifiers, int haveCodepoint, uint32_t codepoint)
{
        struct Input input = {0};
        input.inputKind = INPUT_KEY;
        input.data.tKey.keyKind = keyKind;
        input.data.tKey.keyEventKind = keyeventKind;
        input.data.tKey.modifierMask = modifiers;
        input.data.tKey.hasCodepoint = haveCodepoint;
        input.data.tKey.codepoint = codepoint;
        enqueue_input(&input);
}

static void send_windowresize_event(int width, int height)
{
        struct Input input;
        input.inputKind = INPUT_WINDOWRESIZE;
        input.data.tWindowresize.width = width;
        input.data.tWindowresize.height = height;
        enqueue_input(&input);
}


#define LENGTH(a) (sizeof (a) / sizeof (a)[0])

static const int initialWindowWidth = 800;
static const int initialWindowHeight = 600;

static GLint att[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        GLX_SAMPLE_BUFFERS, 1, // <-- MSAA
        GLX_SAMPLES, 4, // <-- MSAA
        None,
};

static Display *display;
static int screen;
static Window rootWin;
static Window window;
static XVisualInfo *visualInfo;
static Colormap colormap;
static GLXContext contextGlx;

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
        XCloseDisplay(display);
}

static int xbutton_to_mousebuttonKind(int xbutton)
{
        if (xbutton == 1)
                return MOUSEBUTTON_1;
        if (xbutton == 2)
                return MOUSEBUTTON_2;
        if (xbutton == 3)
                return MOUSEBUTTON_3;
        return -1;
}

static void handle_x11_button_press_or_release(XButtonEvent *xbutton, int mousebuttoneventKind)
{
        if (xbutton->button == 4 || xbutton->button == 5) {
                if (mousebuttoneventKind == MOUSEBUTTONEVENT_PRESS) {
                        int keyKind = xbutton->button == 4 ? KEY_SCROLLUP : KEY_SCROLLDOWN;
                        send_scroll_event(keyKind);
                }
                else {
                        // should not happen normally
                }
        }
        else {
                int mousebuttonKind = xbutton_to_mousebuttonKind(xbutton->button);
                send_mousebutton_event(mousebuttonKind, mousebuttoneventKind);
        }
}

void fetch_all_pending_events(void)
{
        // XXX
        {
                XWindowAttributes wa;
                XGetWindowAttributes(display, window, &wa);
                send_windowresize_event(wa.width, wa.height);
        }

        while (XPending(display)) {
                XEvent event;
                XNextEvent(display, &event);
                if (event.type == KeyPress) {
                        XKeyEvent *key = &event.xkey;

                        /* TODO: I don't know how to correctly use either
                        XKeycodeToKeysym() or XkbKeycodeToKeysym(), so I'm using
                        the more convenient function XLookupString() which seems
                        to do what I want. But it's probably slower!
                        To find tricks like this, we have to read source like
                        that of the `xev` program, because there is almost no
                        documentation!
                        I've tried to deal with XKB to do it properly but I will
                        be saving it for later. */
                        KeySym keysym;
                        {
                                //struct Timer timer;
                                //start_timer(&timer);
                                XLookupString(key, NULL, 0, &keysym, NULL);
                                //stop_timer(&timer);
                                //report_timer(&timer, "XLookupString()");
                        }
                        // Actually we can make use of XkbKeycodeToKeysym()
                        // because we want to be dealing in symbols, but
                        // ignoring modifiers!
                        KeySym symNoMods = XkbKeycodeToKeysym(display, key->keycode, 0, 0);

                        int keyKind = KEY_NONE;
                        int haveCodepoint = 0;
                        uint32_t codepoint = 0;
                        for (int i = 0; i < LENGTH(keymap); i++) {
                                if (keymap[i].xkeysym == symNoMods) {
                                        keyKind = keymap[i].keyKind;
                                        break;
                                }
                        }

                        /* directly encoded Unicode */
                        if (0x01000100 <= keysym && keysym <= 0x0110FFFF) {
                                haveCodepoint = 1;
                                codepoint = keysym - 0x01000000;
                        }
                        else {
                                for (int i = 0; i < LENGTH(codepointmap); i++) {
                                        if (codepointmap[i].xkeysym == keysym) {
                                                haveCodepoint = 1;
                                                codepoint = codepointmap[i].codepoint;
                                                break;
                                        }
                                }
                        }
                        int modifiers = 0;
                        if (key->state & ControlMask)
                                modifiers |= MODIFIER_CONTROL;
                        if (key->state & Mod1Mask)
                                modifiers |= MODIFIER_MOD;
                        if (key->state & ShiftMask)
                                modifiers |= MODIFIER_SHIFT;
                        send_key_event(keyKind, KEYEVENT_PRESS, modifiers, haveCodepoint, codepoint);
                }
                else if (event.type == MotionNotify) {
                        XMotionEvent *motion = &event.xmotion;
                        int x = motion->x;
                        int y = motion->y;
                        send_mousemove_event(x, y);
                }
                else if (event.type == ButtonPress) {
                        XButtonEvent *button = &event.xbutton;
                        handle_x11_button_press_or_release(button, MOUSEBUTTONEVENT_PRESS);
                }
                else if (event.type == ButtonRelease) {
                        XButtonEvent *button = &event.xbutton;
                        handle_x11_button_press_or_release(button, MOUSEBUTTONEVENT_RELEASE);
                }
        }
}

void wait_for_events(void)
{
        // TODO
        fetch_all_pending_events();
}

void set_window_title(const char *title)
{
        XStoreName(display, window, title);
}

void toggle_fullscreen(void)
{
        // TODO
}

ANY_FUNCTION *window_get_OpenGL_function_pointer(const char *name)
{
        return glXGetProcAddress((const GLubyte *) name);
}

void swap_buffers(void)
{
        glXSwapBuffers(display, window);
}