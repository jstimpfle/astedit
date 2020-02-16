#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>

extern Display *display;
extern Window window;

void swap_buffers(void)
{
        glXSwapBuffers(display, window);
}
