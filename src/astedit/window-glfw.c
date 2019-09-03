#include <astedit/astedit.h>
#include <astedit/window.h>
#include <astedit/logging.h>
#include <GLFW/glfw3.h>

static const struct {
        int glfwKey;
        int keyKind;
} keymap[] = {
        { GLFW_KEY_BACKSPACE, KEY_BACKSPACE   },
        { GLFW_KEY_DELETE,    KEY_DELETE      },
        { GLFW_KEY_ENTER,     KEY_ENTER       },
        { GLFW_KEY_TAB,       KEY_TAB         },
        { GLFW_KEY_UP,        KEY_CURSORUP    },
        { GLFW_KEY_DOWN,      KEY_CURSORDOWN  },
        { GLFW_KEY_LEFT,      KEY_CURSORLEFT  },
        { GLFW_KEY_RIGHT,     KEY_CURSORRIGHT },
        { GLFW_KEY_PAGE_UP,   KEY_PAGEUP      },
        { GLFW_KEY_PAGE_DOWN, KEY_PAGEDOWN    },
        { GLFW_KEY_HOME,      KEY_HOME        },
        { GLFW_KEY_END,       KEY_END         },
        { GLFW_KEY_ESCAPE,    KEY_ESCAPE      },
        { GLFW_KEY_F1,        KEY_F1 },
        { GLFW_KEY_F2,        KEY_F2 },
        { GLFW_KEY_F3,        KEY_F3 },
        { GLFW_KEY_F4,        KEY_F4 },
        { GLFW_KEY_F5,        KEY_F5 },
        { GLFW_KEY_F6,        KEY_F6 },
        { GLFW_KEY_F7,        KEY_F7 },
        { GLFW_KEY_F8,        KEY_F8 },
        { GLFW_KEY_F9,        KEY_F9 },
        { GLFW_KEY_F10,       KEY_F10 },
        { GLFW_KEY_F11,       KEY_F11 },
        { GLFW_KEY_F12,       KEY_F12 },
        { GLFW_KEY_KP_ADD,      KEY_PLUS },
        { GLFW_KEY_KP_SUBTRACT, KEY_MINUS },
        { GLFW_KEY_A, KEY_A },
        { GLFW_KEY_B, KEY_B },
        { GLFW_KEY_C, KEY_C },
        { GLFW_KEY_D, KEY_D },
        { GLFW_KEY_E, KEY_E },
        { GLFW_KEY_F, KEY_F },
        { GLFW_KEY_G, KEY_G },
        { GLFW_KEY_H, KEY_H },
        { GLFW_KEY_I, KEY_I },
        { GLFW_KEY_J, KEY_J },
        { GLFW_KEY_K, KEY_K },
        { GLFW_KEY_L, KEY_L },
        { GLFW_KEY_M, KEY_M },
        { GLFW_KEY_N, KEY_N },
        { GLFW_KEY_O, KEY_O },
        { GLFW_KEY_P, KEY_P },
        { GLFW_KEY_Q, KEY_Q },
        { GLFW_KEY_R, KEY_R },
        { GLFW_KEY_S, KEY_S },
        { GLFW_KEY_T, KEY_T },
        { GLFW_KEY_U, KEY_U },
        { GLFW_KEY_V, KEY_V },
        { GLFW_KEY_W, KEY_W },
        { GLFW_KEY_X, KEY_X },
        { GLFW_KEY_Y, KEY_Y },
        { GLFW_KEY_Z, KEY_Z },
};

static const struct {
        int glfwMousebutton;
        enum MousebuttonKind mousebutton;
} glfwMousebuttonToMousebutton[] = {
        { GLFW_MOUSE_BUTTON_1, MOUSEBUTTON_1 },
        { GLFW_MOUSE_BUTTON_2, MOUSEBUTTON_2 },
        { GLFW_MOUSE_BUTTON_3, MOUSEBUTTON_3 },
        { GLFW_MOUSE_BUTTON_4, MOUSEBUTTON_4 },
        { GLFW_MOUSE_BUTTON_5, MOUSEBUTTON_5 },
        { GLFW_MOUSE_BUTTON_6, MOUSEBUTTON_6 },
        { GLFW_MOUSE_BUTTON_7, MOUSEBUTTON_7 },
        { GLFW_MOUSE_BUTTON_8, MOUSEBUTTON_8 },
};

static const struct {
        int glfwAction;
        enum KeyEventKind keyeventKind;
} glfwActionToKeyeventKind[] = {
        { GLFW_PRESS, KEYEVENT_PRESS },
        { GLFW_REPEAT, KEYEVENT_REPEAT },
        { GLFW_RELEASE, KEYEVENT_RELEASE },
};



static GLFWwindow *windowGlfw;
static int isFullscreenMode;

static volatile int doingPolling;  // needed for a hack. See below



static void error_cb_glfw(int err, const char *msg)
{
        fatalf("Error %d from GLFW: %s\n", err, msg);
}

static int glfwmods_to_modifiers(int mods)
{
        int modifiers = 0;
        if (mods & GLFW_MOD_ALT)
                modifiers |= MODIFIER_MOD;
        if (mods & GLFW_MOD_CONTROL)
                modifiers |= MODIFIER_CONTROL;
        if (mods & GLFW_MOD_SHIFT)
                modifiers |= MODIFIER_SHIFT;
        return modifiers;
}


static void enqueue_key_input(
        enum KeyKind keyKind, enum KeyEventKind keyEventKind,
        int modifierMask, int hasCodepoint, unsigned codepoint)
{
        struct Input inp;
        inp.inputKind = INPUT_KEY;
        inp.data.tKey.keyKind = keyKind;
        inp.data.tKey.keyEventKind = keyEventKind;
        inp.data.tKey.modifierMask = modifierMask;
        inp.data.tKey.hasCodepoint = hasCodepoint;
        inp.data.tKey.codepoint = codepoint;
        enqueue_input(&inp);
}

static void mouse_cb_glfw(GLFWwindow *win, int button, int action, int mods)
{
        (void)win;
        enum MousebuttonEventKind mousebuttonEventKind;

        if (action == GLFW_PRESS)
                mousebuttonEventKind = MOUSEBUTTONEVENT_PRESS;
        else if (action == GLFW_RELEASE)
                mousebuttonEventKind = MOUSEBUTTONEVENT_RELEASE;
        else
                return;

        for (int i = 0; i < LENGTH(glfwMousebuttonToMousebutton); i++) {
                if (button == glfwMousebuttonToMousebutton[i].glfwMousebutton) {
                        enum MousebuttonKind mousebuttonKind = glfwMousebuttonToMousebutton[i].mousebutton;
                        struct Input inp;
                        inp.inputKind = INPUT_MOUSEBUTTON;
                        inp.data.tMousebutton.mousebuttonKind = mousebuttonKind;
                        inp.data.tMousebutton.mousebuttonEventKind = mousebuttonEventKind;
                        inp.data.tMousebutton.modifiers = glfwmods_to_modifiers(mods);
                        enqueue_input(&inp);
                }
        }
}

static void cursor_cb_glfw(GLFWwindow *win, double xoff, double yoff)
{
        UNUSED(win);
        struct Input inp;
        inp.inputKind = INPUT_CURSORMOVE;
        inp.data.tCursormove.pixelX = (int)xoff;
        inp.data.tCursormove.pixelY = (int)yoff;
        enqueue_input(&inp);
}

static void scroll_cb_glfw(GLFWwindow *win, double xoff, double yoff)
{
        (void)win;

        enum KeyKind keyKind;
        if (xoff < 0.0)
                keyKind = KEY_SCROLLLEFT;
        else if (xoff > 0.0)
                keyKind = KEY_SCROLLRIGHT;
        else if (yoff < 0.0)
                keyKind = KEY_SCROLLDOWN;
        else if (yoff > 0.0)
                keyKind = KEY_SCROLLUP;
        else
                return;

        int modifiers = 0;
        int hasCodepoint = 0;
        unsigned codepoint = 0;
        enqueue_key_input(keyKind, KEYEVENT_PRESS, modifiers, hasCodepoint, codepoint);
}

static void key_cb_glfw(GLFWwindow *win, int key, int scancode, int action, int mods)
{
        (void)win;
        (void)scancode;

        for (int i = 0; i < LENGTH(keymap); i++) {
                if (key == keymap[i].glfwKey) {
                        enum KeyKind keyKind = keymap[i].keyKind;
                        enum KeyEventKind keyeventKind = -1;
                        for (int j = 0; j < LENGTH(glfwActionToKeyeventKind); j++)
                                if (glfwActionToKeyeventKind[j].glfwAction == action)
                                        keyeventKind = glfwActionToKeyeventKind[j].keyeventKind;
                        ENSURE(keyeventKind != -1);
                        int modifiers = glfwmods_to_modifiers(mods);
                        int hasCodepoint = 0;
                        unsigned codepoint = 0;
                        enqueue_key_input(keyKind, keyeventKind, modifiers, hasCodepoint, codepoint);
                        return;
                }
        }
}

static void char_cb_glfw(GLFWwindow *win, unsigned int codepoint)
{
        (void)win;
        enum KeyKind keyKind = KEY_NONE;
        int modifiers = 0;
        if (65 <= codepoint && codepoint <= 90) {
                keyKind = KEY_A + codepoint - 65;
                modifiers = MODIFIER_SHIFT;
        }
        if (97 <= codepoint && codepoint <= 122) {
                keyKind = KEY_A + codepoint - 97;
                modifiers = 0;
        }
        int hasCodepoint = 1;
        enqueue_key_input(keyKind, KEYEVENT_PRESS, modifiers, hasCodepoint, codepoint);
}

static void windowsize_cb_glfw(GLFWwindow *win, int width, int height)
{
        (void)win;
        ENSURE(win == windowGlfw);
        if (!width && !height) {
                /* for now, this is our weird workaround against division
                by 0 / NaN problems resulting from window minimization. */
                return;
        }

        windowWidthInPixels = width;
        windowHeightInPixels = height;

        struct Input inp;
        inp.inputKind = INPUT_WINDOWRESIZE;
        inp.data.tWindowresize.width = width;
        inp.data.tWindowresize.height = height;
        enqueue_input(&inp);

        //XXX: there's an issue that glfwPollEvents() blocks on some platforms
        // during a window move or resize (see notes in GLFW docs).
        // To work around that, for now we just duplicate drawing in this callback.
        // it's not nice and likely to break, so consider it a temporary hack.
        if (doingPolling) {
                extern long long timeSinceProgramStartupMilliseconds;
                extern void mainloop(void);
                if (timeSinceProgramStartupMilliseconds > 2000) { // XXX OpenGL should be set up by now
                        mainloop();
                }
        }
}

static void windowrefresh_cb_glfw(GLFWwindow *win)
{
        (void)win;
        ENSURE(win == windowGlfw);
        struct Input inp;
        inp.inputKind = INPUT_WINDOWEXPOSE;
        enqueue_input(&inp);
}

void enter_windowing_mode(void)
{
        GLFWmonitor *monitor = NULL;
        int pixelsW = 1280;
        int pixelsH = 1024;
        glfwSetWindowMonitor(windowGlfw, monitor, 300, 300, pixelsW, pixelsH, GLFW_DONT_CARE);
        isFullscreenMode = 0;
}

void enter_fullscreen_mode(void)
{
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();  // try full screen mode. Will stuttering go away?
        const struct GLFWvidmode *mode = glfwGetVideoMode(monitor);
        int pixelsW = mode->width;
        int pixelsH = mode->height;
        glfwSetWindowMonitor(windowGlfw, monitor, 0, 0, pixelsW, pixelsH, GLFW_DONT_CARE);
        isFullscreenMode = 1;
}

void toggle_fullscreen(void)
{
        if (isFullscreenMode)
                enter_windowing_mode();
        else
                enter_fullscreen_mode();
}

void setup_window(void)
{
        glfwSetErrorCallback(&error_cb_glfw);

        if (glfwInit() != GLFW_TRUE)
                fatal("Failed to initialize glfw!\n");

        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_SAMPLES, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        //glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
        //glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);  // window size dependent on monitor scale


        windowGlfw = glfwCreateWindow(1024, 768, "Astedit", NULL, NULL);
        if (!windowGlfw)
                fatal("Failed to create GLFW window\n");

        //enter_fullscreen_mode();
        enter_windowing_mode();

        glfwSetMouseButtonCallback(windowGlfw, &mouse_cb_glfw);
        glfwSetScrollCallback(windowGlfw, &scroll_cb_glfw);
        glfwSetCursorPosCallback(windowGlfw, &cursor_cb_glfw);
        glfwSetKeyCallback(windowGlfw, &key_cb_glfw);
        glfwSetCharCallback(windowGlfw, &char_cb_glfw);
        glfwSetFramebufferSizeCallback(windowGlfw, &windowsize_cb_glfw);
        glfwSetWindowRefreshCallback(windowGlfw, &windowrefresh_cb_glfw);

        { /* call the callback artificially */
                int pixX, pixY;
                glfwGetWindowSize(windowGlfw, &pixX, &pixY);
                windowsize_cb_glfw(windowGlfw, pixX, pixY);
        }

        glfwMakeContextCurrent(windowGlfw);
}

void teardown_window(void)
{

        glfwMakeContextCurrent(NULL);
        glfwDestroyWindow(windowGlfw);
        glfwTerminate();
}

void wait_for_events(void)
{
        doingPolling = 1;
        // TODO: Use glfwPollEvents() if it's clear that we should produce the next frame immediately
        if (0) {
                glfwWaitEvents();
        }
        else {
                glfwPollEvents();
        }
        doingPolling = 0;

        if (glfwWindowShouldClose(windowGlfw))
                shouldWindowClose = 1;

        // for now, also generate a virtual tick event
        {
                struct Input inp;
                inp.inputKind = INPUT_TIMETICK;
                enqueue_input(&inp);
        }
}

void swap_buffers(void)
{
        glfwSwapInterval(1);
        glfwSwapBuffers(windowGlfw);
}
