#include <astedit/astedit.h>
#include <astedit/logging.h>
//#include <astedit/opengl.h>
#include <astedit/window.h>
#include <assert.h>
#include <Windows.h>
#include <windowsx.h>  // GET_X_LPARAM(), GET_Y_LPARAM()
#include <ShellScalingApi.h>  // SetProcessDpiAwareness()
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>


static const struct {
        unsigned code;
        enum KeyKind keyKind;
} keyMap[] = {
        { VK_RETURN, KEY_ENTER },
        { VK_BACK, KEY_BACKSPACE },
        { VK_ESCAPE, KEY_ESCAPE },
        { VK_LEFT, KEY_CURSORLEFT },
        { VK_RIGHT, KEY_CURSORRIGHT },
        { VK_UP, KEY_CURSORUP },
        { VK_DOWN, KEY_CURSORDOWN },
        { VK_SPACE, KEY_SPACE },
        { VK_OEM_MINUS, KEY_MINUS },
        { VK_OEM_PLUS, KEY_PLUS },
        { VK_DELETE, KEY_DELETE },
        { VK_PRIOR, KEY_PAGEUP },
        { VK_NEXT, KEY_PAGEDOWN },
        { VK_HOME, KEY_HOME },
        { VK_END, KEY_END },
        { VK_TAB, KEY_TAB },
        { VK_F1, KEY_F1 },
        { VK_F2, KEY_F2 },
        { VK_F3, KEY_F3 },
        { VK_F4, KEY_F4 },
        { VK_F5, KEY_F5 },
        { VK_F6, KEY_F6 },
        { VK_F7, KEY_F7 },
        { VK_F8, KEY_F8 },
        { VK_F9, KEY_F9 },
        { VK_F10, KEY_F10 },
        { VK_F11, KEY_F11 },
        { VK_F12, KEY_F12 },
        { 0x30, KEY_0 },
        { 0x31, KEY_1 },
        { 0x32, KEY_2 },
        { 0x33, KEY_3 },
        { 0x34, KEY_4 },
        { 0x36, KEY_5 },
        { 0x36, KEY_6 },
        { 0x37, KEY_7 },
        { 0x38, KEY_8 },
        { 0x39, KEY_9 },
        { 0x41, KEY_A },
        { 0x42, KEY_B },
        { 0x43, KEY_C },
        { 0x44, KEY_D },
        { 0x45, KEY_E },
        { 0x46, KEY_F },
        { 0x47, KEY_G },
        { 0x48, KEY_H },
        { 0x49, KEY_I },
        { 0x4a, KEY_J },
        { 0x4b, KEY_K },
        { 0x4c, KEY_L },
        { 0x4d, KEY_M },
        { 0x4e, KEY_N },
        { 0x4f, KEY_O },
        { 0x50, KEY_P },
        { 0x51, KEY_Q },
        { 0x52, KEY_R },
        { 0x53, KEY_S },
        { 0x54, KEY_Z },
        { 0x55, KEY_U },
        { 0x56, KEY_V },
        { 0x57, KEY_W },
        { 0x58, KEY_X },
        { 0x59, KEY_Y },
        { 0x5a, KEY_Z },
};

static const struct {
        int code;
        int mousebuttonKind;
} mouseMap[] = {
        { VK_LBUTTON, MOUSEBUTTON_1 },
        { VK_MBUTTON, MOUSEBUTTON_2 },
        { VK_RBUTTON, MOUSEBUTTON_3 },
};

static HWND globalWND;
static HDC globalDC;
static HGLRC globalGLRC;

static void update_window_dimensions(void)
{
        RECT rect;
        if (GetClientRect(globalWND, &rect)) {
                int w = rect.right;
                int h = rect.bottom;
                // this seems to avoid some issues...
                if (!w) w = 1;
                if (!h) h = 1;
                enqueue_windowsize_input(w, h);
        }
}

static int get_modifiers(void)
{
        int modifiers = 0;
        if (GetKeyState(VK_CONTROL) & 0x8000)
                modifiers |= MODIFIER_CONTROL;
        if (GetKeyState(VK_MENU) & 0x8000)
                modifiers |= MODIFIER_MOD;
        if (GetKeyState(VK_SHIFT) & 0x8000)
                modifiers |= MODIFIER_SHIFT;
        return modifiers;
}

LRESULT CALLBACK my_window_proc(
    _In_ HWND hWnd,
    _In_ UINT msg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam)
{
        if (msg == WM_CREATE) {
                return TRUE;
        }
        else if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
                enum KeyKind keyKind = -1;
                for (int i = 0; i < sizeof keyMap / sizeof keyMap[0]; i++) {
                        if (wParam == keyMap[i].code) {
                                keyKind = keyMap[i].keyKind;
                                break;
                        }
                }
                if (keyKind == -1)
                        return FALSE;
                int repeated = lParam & (1 << 30);
                enum KeyeventKind keyeventKind = repeated ? KEYEVENT_REPEAT : KEYEVENT_PRESS;
                int modifiers = get_modifiers();
                int hasCodepoint = 0; // TODO
                uint32_t codepoint = (uint32_t) -1;
                if (keyKind == KEY_F4 && modifiers == MODIFIER_MOD)
                        return DefWindowProcA(hWnd, msg, wParam, lParam);
                enqueue_key_input(keyKind, keyeventKind, modifiers,
                        hasCodepoint, codepoint);
                return TRUE;
        }
        else if (msg == WM_CHAR) {
                int keyKind = -1; //XXX
                int repeated = lParam & (1 << 30);
                enum KeyeventKind keyeventKind = repeated ? KEYEVENT_REPEAT : KEYEVENT_PRESS;
                int modifiers = 0; //XXX
                int hasCodepoint = 1;
                uint32_t codepoint = (uint32_t) wParam; //XXX what does wParam contain exactly?
                if (codepoint == 8 || codepoint == 9 || codepoint == 10 || codepoint == 13) {
                        /* ignore some codepoints < 32. We already received a WM_KEYDOWN event that results in
                        KEY_BACKSPACE / KEY_ENTER / KEY_TAB etc.
                        Windows key handling is the worst design ever. It's literally impossible
                        to do it correctly because for some keys we receive WM_KEYDOWN as well
                        as WM_CHAR messages and it's literally impossible to correlate them to
                        make sure only one of them is handled. */
                }
                else {
                        enqueue_key_input(keyKind, keyeventKind, modifiers,
                                hasCodepoint, codepoint);
                }
                return TRUE;
        }
        else if (msg == WM_LBUTTONDOWN) {
                enqueue_mousebutton_input(MOUSEBUTTON_1, MOUSEBUTTONEVENT_PRESS, 0);
                return TRUE;
        }
        else if (msg == WM_LBUTTONUP) {
                enqueue_mousebutton_input(MOUSEBUTTON_1, MOUSEBUTTONEVENT_RELEASE, 0);
                return TRUE;
        }
        else if (msg == WM_MBUTTONDOWN) {
                enqueue_mousebutton_input(MOUSEBUTTON_2, MOUSEBUTTONEVENT_PRESS, 0);
                return TRUE;
        }
        else if (msg == WM_MBUTTONDOWN) {
                enqueue_mousebutton_input(MOUSEBUTTON_2, MOUSEBUTTONEVENT_RELEASE, 0);
                return TRUE;
        }
        else if (msg == WM_RBUTTONDOWN) {
                enqueue_mousebutton_input(MOUSEBUTTON_3, MOUSEBUTTONEVENT_PRESS, 0);
                return TRUE;
        }
        else if (msg == WM_RBUTTONDOWN) {
                enqueue_mousebutton_input(MOUSEBUTTON_3, MOUSEBUTTONEVENT_RELEASE, 0);
                return TRUE;
        }
        else if (msg == WM_MOUSEMOVE) {
                enqueue_cursormove_input(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return TRUE;
        }
        else if (msg == WM_MOUSEWHEEL) {
                float direction = GET_WHEEL_DELTA_WPARAM(wParam) / (float) WHEEL_DELTA;
                enum KeyKind keyKind = direction > 0 ? KEY_SCROLLUP : KEY_SCROLLDOWN;
                int modifiers = get_modifiers();
                enqueue_key_input(keyKind, KEYEVENT_PRESS, modifiers, 0, 0);
                return TRUE;
        }
        else if (msg == WM_WINDOWPOSCHANGED) {
                update_window_dimensions();
                return TRUE;
        }
        else if (msg == WM_SIZE) {
                update_window_dimensions();
                return TRUE;
        }
        else if (msg == WM_SIZING) {
                update_window_dimensions();
                return TRUE;
        }
        else if (msg == WM_CLOSE) {
                exit(0); //XXX
        }
        else {
                return DefWindowProcA(hWnd, msg, wParam, lParam);
        }
}

static PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
static PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;

/* This function copied from https://gist.github.com/nickrolfe/1127313ed1dbf80254b614a721b3ee9c */
static void init_opengl_extensions(void)
{
        // Before we can load extensions, we need a dummy OpenGL context, created using a dummy window.
        // We use a dummy window because you can only set the pixel format for a window once. For the
        // real window, we want to use wglChoosePixelFormatARB (so we can potentially specify options
        // that aren't available in PIXELFORMATDESCRIPTOR), but we can't load and use that before we
        // have a context.
        WNDCLASSA window_class = {
            .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
            .lpfnWndProc = DefWindowProcA,
            .hInstance = GetModuleHandle(0),
            .lpszClassName = "Dummy_WGL_djuasiodwa",
        };

        if (!RegisterClassA(&window_class))
                fatalf("Failed to register dummy OpenGL window.");

        HWND dummy_window = CreateWindowExA(
                0,
                window_class.lpszClassName,
                "Dummy OpenGL Window",
                0,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                0,
                0,
                window_class.hInstance,
                0);

        if (!dummy_window)
                fatalf("Failed to create dummy OpenGL window.");

        HDC dummy_dc = GetDC(dummy_window);

        PIXELFORMATDESCRIPTOR pfd = {
            .nSize = sizeof(pfd),
            .nVersion = 1,
            .iPixelType = PFD_TYPE_RGBA,
            .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            .cColorBits = 32,
            .cAlphaBits = 8,
            .iLayerType = PFD_MAIN_PLANE,
            .cDepthBits = 24,
            .cStencilBits = 8,
        };

        int pixel_format = ChoosePixelFormat(dummy_dc, &pfd);
        if (!pixel_format)
                fatalf("Failed to find a suitable pixel format.");
        if (!SetPixelFormat(dummy_dc, pixel_format, &pfd))
                fatalf("Failed to set the pixel format.");

        HGLRC dummy_context = wglCreateContext(dummy_dc);
        if (!dummy_context)
                fatalf("Failed to create a dummy OpenGL rendering context.");
        if (!wglMakeCurrent(dummy_dc, dummy_context))
                fatalf("Failed to activate dummy OpenGL rendering context.");

        wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
        wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
        if (wglCreateContextAttribsARB == NULL)
                fatalf("failed to load extension wglCreateContextAttribsARB()");
        if (wglChoosePixelFormatARB == NULL)
                fatalf("failed to load extension arglChoosePixelFormatARB()");

        wglMakeCurrent(dummy_dc, 0);
        wglDeleteContext(dummy_context);
        ReleaseDC(dummy_window, dummy_dc);
        DestroyWindow(dummy_window);
}

void setup_window(void)
{
        /* Please, dear Windows, don't mess with my pixels. */
        SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);

        init_opengl_extensions();

        HMODULE hInstance = GetModuleHandle(NULL);
        if (hInstance == NULL)
                fatalf("Failed to GetModuleHandle(NULL)");
        int nWidth = 640;
        int nHeight = 640;
        WNDCLASSA wc      = {0};
        wc.lpfnWndProc   = my_window_proc;
        wc.hInstance     = hInstance;
        wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
        wc.lpszClassName = "myclass";
        wc.style = CS_OWNDC;
        if (!RegisterClassA(&wc))
                fatalf("Failed to register window class");
        globalWND = CreateWindowA(wc.lpszClassName, "My Window", WS_OVERLAPPEDWINDOW|WS_VISIBLE, 100, 100, nWidth, nHeight, NULL, NULL, hInstance, NULL);
        if (globalWND == NULL)
                fatalf("Failed to create window");

        update_window_dimensions();

        globalDC = GetDC(globalWND);
        if (globalDC == NULL)
                fatalf("Failed to GetDC() from HWND");

        /*
        FIND AND SET PIXEL FORMAT
        */

        int trysamplesettings[] = {
                16, 4, 1
        };
        int pixelFormat = 0; // initialize for the compiler
        int foundPixelFormat = 0;

        for (int i = 0; i < LENGTH(trysamplesettings); i++) {
                int numSamples = trysamplesettings[i];
                const float pfAttribFList[] = { 0, 0 };
                const int piAttribIList[] = {
                    WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
                    WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
                    WGL_COLOR_BITS_ARB, 32,
                    WGL_RED_BITS_ARB, 8,
                    WGL_GREEN_BITS_ARB, 8,
                    WGL_BLUE_BITS_ARB, 8,
                    WGL_ALPHA_BITS_ARB, 8,
                    WGL_DEPTH_BITS_ARB, 16,
                    WGL_STENCIL_BITS_ARB, 0,
                    WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
                    WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
                    WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
                    WGL_SAMPLES_ARB, numSamples,
                    0, 0
                };

                UINT nMaxFormats = 1;
                UINT nNumFormats;
                if (!wglChoosePixelFormatARB(globalDC, piAttribIList, pfAttribFList, nMaxFormats, &pixelFormat, &nNumFormats))
                        continue;
                foundPixelFormat = 1;
                break;
        }

        if (!foundPixelFormat)
                fatalf("Failed to ChoosePixelFormat()");

        /* Passing NULL as the PIXELFORMATDESCRIPTOR pointer. Does that work on all machines?
        The documentation is cryptic on the use of that value.
        In conjunction with wglChoosePixelFormatARB(), the method that you can find on the internet,
        which involves calling DescribePixelFormat() + passing non-NULL parameter here did not work
        on all machines for me. */
        if (!SetPixelFormat(globalDC, pixelFormat, NULL))
                fatalf("failed to SetPixelFormat()");

        /* create opengl context */
        static const int gl30_attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 2,
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0,
        };
        globalGLRC = wglCreateContextAttribsARB(globalDC, 0, gl30_attribs);
        if (!globalGLRC)
                fatalf("Failed to create OpenGL 3.2 context.");

        if (!wglMakeCurrent(globalDC, globalGLRC))
                fatalf("Failed to wglMakeCurrent(globalDC, globalGLRC);");
}

void teardown_window(void)
{
        // TODO
}

void set_window_title(const char *name)
{
        if (!SetWindowTextA(globalWND, name))
                log_postf("Warning: failed to set window title (request was '%s')", name);
}

static WINDOWPLACEMENT previousWindowPlacement;

static void enter_fullscreen(void)
{
        if (!GetWindowPlacement(globalWND, &previousWindowPlacement)) {
                log_postf("Failed to query current window placement. Not entering fullscreen");
                return;
        }
        POINT Point = { 0 };
        HMONITOR Monitor = MonitorFromPoint(Point, MONITOR_DEFAULTTONEAREST);
        MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
        if (GetMonitorInfo(Monitor, &MonitorInfo)) {
                DWORD Style = WS_POPUP | WS_VISIBLE;
                SetWindowLongPtr(globalWND, GWL_STYLE, Style);
                SetWindowPos(globalWND, 0, MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                        MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                        SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        }
}

static void leave_fullscreen(void)
{
        DWORD Style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN;
        SetWindowLongPtr(globalWND, GWL_STYLE, Style);
        if (!SetWindowPlacement(globalWND, &previousWindowPlacement))
                log_postf("Failed to restore previous window placement");
}

void toggle_fullscreen(void)
{
        static int isFullscreen;
        isFullscreen ^= 1;
        if (isFullscreen)
                enter_fullscreen();
        else
                leave_fullscreen();
}

ANY_FUNCTION *window_get_OpenGL_function_pointer(const char *name)
{
        return (ANY_FUNCTION *) wglGetProcAddress(name);
}

void wait_for_events(void)
{
        MSG msg;
        BOOL bRet;
        for (;;) {
                bRet = PeekMessageA(&msg, globalWND, 0, 0, PM_REMOVE);
                if (bRet == 0)
                        break;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
        }
}

void swap_buffers(void)
{
        if (!SwapBuffers(globalDC))
                fatalf("Failed to SwapBuffers()");
}
