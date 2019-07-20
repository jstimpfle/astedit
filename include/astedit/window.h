#ifndef ASTEDIT_WINDOW_H_INCLUDED
#define ASTEDIT_WINDOW_H_INCLUDED

enum KeyKind {
        KEY_NONE = -1,  /* we use this because we have "optional" keys in input handling */

        KEY_ENTER = 0,
        KEY_BACKSPACE,
        KEY_DELETE,
        KEY_CURSORUP,
        KEY_CURSORDOWN,
        KEY_CURSORLEFT,
        KEY_CURSORRIGHT,
        KEY_PAGEUP,
        KEY_PAGEDOWN,
        KEY_HOME,
        KEY_END,
        KEY_ESCAPE,
        KEY_F1,
        KEY_F2,
        KEY_F3,
        KEY_F4,
        KEY_F5,
        KEY_F6,
        KEY_F7,
        KEY_F8,
        KEY_F9,
        KEY_F10,
        KEY_F11,
        KEY_F12,
        KEY_PLUS,
        KEY_MINUS,

        KEY_A,
        KEY_B,
        KEY_C,
        KEY_D,
        KEY_E,
        KEY_F,
        KEY_G,
        KEY_H,
        KEY_I,
        KEY_J,
        KEY_K,
        KEY_L,
        KEY_M,
        KEY_N,
        KEY_O,
        KEY_P,
        KEY_Q,
        KEY_R,
        KEY_S,
        KEY_T,
        KEY_U,
        KEY_V,
        KEY_W,
        KEY_X,
        KEY_Y,
        KEY_Z,

        // we will treat mouse scrolls as "keyboard input"
        KEY_SCROLLLEFT,
        KEY_SCROLLRIGHT,
        KEY_SCROLLUP,
        KEY_SCROLLDOWN,
};

enum MousebuttonKind {
        MOUSEBUTTON_1,
        MOUSEBUTTON_2,
        MOUSEBUTTON_3,
        MOUSEBUTTON_4,
        MOUSEBUTTON_5,
        MOUSEBUTTON_6,
        MOUSEBUTTON_7,
        MOUSEBUTTON_8,
        NUM_MOUSEBUTTON_KINDS,
};

enum InputKind {
        // currently INPUT_KEY and INPUT_MOUSEBUTTON are separate things,
        // even though we could make them one. But I think processing is
        // usually done separately anyway.
        INPUT_KEY,
        INPUT_MOUSEBUTTON,

        INPUT_CURSORMOVE,

        // virtual tick input from time to time for now,
        // so we don't have to have different functions for
        // "user input" and "some time has passed"
        INPUT_TIMETICK,

        INPUT_WINDOWRESIZE,
        INPUT_WINDOWEXPOSE,
        INPUT_WINDOWMAP,
        INPUT_WINDOWUNMAP,
};

enum KeyEventKind {
        KEYEVENT_PRESS,
        KEYEVENT_RELEASE,
        KEYEVENT_REPEAT,
};

enum MousebuttonEventKind {
        MOUSEBUTTONEVENT_PRESS,
        MOUSEBUTTONEVENT_RELEASE,
};

enum {
        MODIFIER_SHIFT = 1 << 0,
        MODIFIER_CONTROL = 1 << 1,
        MODIFIER_MOD = 1 << 2,
};

/* Key handling is unfortunately influenced a little bit by how GLFW works. this should be improved.*/
struct KeyInput {
        /* the keyKind fields contains a KEY_?? value. It might be KEY_NONE if the input is not representable as one of the other KEY_?? values.
        In this case the codepoint field contains an UTF-8 character. */
        enum KeyKind keyKind;  // KEY_??. Might be KEY_NONE in case of an unicode character. In this case the codepoint field should be meaningful.
        enum KeyEventKind keyEventKind;

        int modifiers;  // OR'ed MODIFIER_??'s. Only valid if keyKind != KEY_NONE
        int hasCodepoint;
        unsigned long codepoint;
};

struct MousebuttonInput {
        enum MousebuttonKind mousebuttonKind;
        enum MousebuttonEventKind mousebuttonEventKind;
        int modifiers;  // OR'ed MODIFIER_??'s
};

struct CursormoveInput {
        int pixelX;
        int pixelY;
};

struct WindowresizeInput {
        int width;
        int height;
};

struct Input {
        int inputKind;
        union {
                struct KeyInput tKey;
                struct MousebuttonInput tMousebutton;
                struct CursormoveInput tCursormove;
                struct WindowresizeInput tWindowresize;
        };
};




DATA int shouldWindowClose;

DATA int windowWidthInPixels;
DATA int windowHeightInPixels;



void enqueue_input(const struct Input *input);
int look_input(struct Input *input);
void consume_input(void);

/* implemented in backend-specific file (window-XXX.c) */
void setup_window(void);
void teardown_window(void);
void wait_for_events(void);
void swap_buffers(void);

#endif
