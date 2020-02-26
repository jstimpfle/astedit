#include <astedit/logging.h>
#include <astedit/window.h>
#include <astedit/x11.h>
#include <X11/Xlib.h>

static const int initialWindowWidth = 800;
static const int initialWindowHeight = 600;

void setup_window(void)
{
        display = XOpenDisplay(NULL);
        if (display == NULL)
                fatalf("Failed to XOpenDisplay()");

        screen = DefaultScreen(display);
        rootWin = DefaultRootWindow(display);
        visual = DefaultVisual(display, screen);
        depth = DefaultDepth(display, screen);

        log_postf("depth is %d", depth);

        XSetWindowAttributes wa = {0};
        wa.event_mask = ExposureMask
                | KeyPressMask | KeyReleaseMask
                | ButtonPressMask | ButtonReleaseMask
                | PointerMotionMask;

        window = XCreateWindow(display, rootWin, 0, 0,
                               initialWindowWidth, initialWindowHeight,
                               0, depth,
                               InputOutput, visual,
                               CWEventMask, &wa);

        XMapWindow(display, window);
        XStoreName(display, window, "Untitled Window");
}

void teardown_window(void)
{
        XDestroyWindow(display, window);
        XCloseDisplay(display);
}
