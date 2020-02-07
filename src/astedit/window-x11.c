#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <astedit/window.h>

#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h> // glXCreateContextAttribsARB

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

static void send_key_event(int keyKind, int keyeventKind)
{
        struct Input input = {0};
        input.inputKind = INPUT_KEY;
        input.data.tKey.keyKind = keyKind;
        input.data.tKey.keyEventKind = keyeventKind;
        enqueue_input(&input);
}

static void send_codepoint_event(int keyKind, uint32_t codepoint)
{
        // XXX
        struct Input input = {0};
        input.inputKind = INPUT_KEY;
        input.data.tKey.keyKind = KEY_NONE;
        input.data.tKey.keyEventKind = KEYEVENT_PRESS;
        input.data.tKey.hasCodepoint = 1;
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

static const struct {
        int xkeysym;
        int keyKind;
} keymap[] = {
        { XK_Escape, KEY_ESCAPE },
        { XK_BackSpace, KEY_BACKSPACE },
        { XK_Left, KEY_CURSORLEFT },
        { XK_Right, KEY_CURSORRIGHT },
        { XK_Up, KEY_CURSORUP },
        { XK_Down, KEY_CURSORDOWN },
};

// urrrghh... What's a good mechanism to map keys to unicode?
static const struct {
        int xkeysym;
        int keyKind;
        uint32_t codepoint;
} codepointmap[] = {
        { XK_A, KEY_A, 'A' },
        { XK_B, KEY_B, 'B' },
        { XK_C, KEY_C, 'C' },
        { XK_D, KEY_D, 'D' },
        { XK_E, KEY_E, 'E' },
        { XK_F, KEY_F, 'F' },
        { XK_G, KEY_G, 'G' },
        { XK_H, KEY_H, 'H' },
        { XK_I, KEY_I, 'I' },
        { XK_J, KEY_J, 'J' },
        { XK_K, KEY_K, 'K' },
        { XK_L, KEY_L, 'L' },
        { XK_M, KEY_M, 'M' },
        { XK_N, KEY_N, 'N' },
        { XK_O, KEY_O, 'O' },
        { XK_P, KEY_P, 'P' },
        { XK_Q, KEY_Q, 'Q' },
        { XK_R, KEY_R, 'R' },
        { XK_S, KEY_S, 'S' },
        { XK_T, KEY_T, 'T' },
        { XK_U, KEY_U, 'U' },
        { XK_W, KEY_W, 'W' },
        { XK_X, KEY_X, 'X' },
        { XK_Y, KEY_Y, 'Y' },
        { XK_Z, KEY_Z, 'Z' },
        { XK_a, KEY_A, 'a' },
        { XK_b, KEY_B, 'b' },
        { XK_c, KEY_C, 'c' },
        { XK_d, KEY_D, 'd' },
        { XK_e, KEY_E, 'e' },
        { XK_f, KEY_F, 'f' },
        { XK_g, KEY_G, 'g' },
        { XK_h, KEY_H, 'h' },
        { XK_i, KEY_I, 'i' },
        { XK_j, KEY_J, 'j' },
        { XK_k, KEY_K, 'k' },
        { XK_l, KEY_L, 'l' },
        { XK_m, KEY_M, 'm' },
        { XK_n, KEY_N, 'n' },
        { XK_o, KEY_O, 'o' },
        { XK_p, KEY_P, 'p' },
        { XK_q, KEY_Q, 'q' },
        { XK_r, KEY_R, 'r' },
        { XK_s, KEY_S, 's' },
        { XK_t, KEY_T, 't' },
        { XK_u, KEY_U, 'u' },
        { XK_w, KEY_W, 'w' },
        { XK_x, KEY_X, 'x' },
        { XK_y, KEY_Y, 'y' },
        { XK_z, KEY_Z, 'z' },
        { XK_0, KEY_0, '0' },
        { XK_1, KEY_1, '1' },
        { XK_2, KEY_2, '2' },
        { XK_3, KEY_3, '3' },
        { XK_4, KEY_4, '4' },
        { XK_5, KEY_5, '5' },
        { XK_6, KEY_6, '6' },
        { XK_7, KEY_7, '7' },
        { XK_8, KEY_8, '8' },
        { XK_9, KEY_9, '9' },
        { XK_Adiaeresis, KEY_NONE, 0xC4 },
        { XK_adiaeresis, KEY_NONE, 0xE4 },
        { XK_Odiaeresis, KEY_NONE, 0xD6 },
        { XK_odiaeresis, KEY_NONE, 0xF6 },
        { XK_Udiaeresis, KEY_NONE, 0xDC },
        { XK_udiaeresis, KEY_NONE, 0xFC },
        { XK_ssharp, KEY_NONE, 0xDF },
        { XK_EuroSign, KEY_NONE, 0x20 },

        { XK_Return, KEY_ENTER, '\n' },
        { XK_space, KEY_SPACE, ' ' },
        { XK_comma, KEY_NONE, ',' },
        { XK_period, KEY_NONE, '.' },
        { XK_colon, KEY_NONE, ':' },
        { XK_semicolon, KEY_NONE, ';' },
};

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
                        int keysym = XkbKeycodeToKeysym(display, key->keycode, 0, key->state);
                        //int keysym = XKeycodeToKeysym(display, key->keycode, key->state);
                        log_postf("keysym is %d", keysym);
                        for (int i = 0; i < LENGTH(keymap); i++) {
                                if (keymap[i].xkeysym == keysym) {
                                        send_key_event(keymap[i].keyKind, KEYEVENT_PRESS);
                                        goto ok;
                                }
                        }
                        for (int i = 0; i < LENGTH(codepointmap); i++) {
                                if (codepointmap[i].xkeysym == keysym) {
                                        log_postf("Got '%c'", codepointmap[i].codepoint);
                                        send_codepoint_event(codepointmap[i].keyKind,
                                                             codepointmap[i].codepoint);
                                }
                        }
ok:
                        ;
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
