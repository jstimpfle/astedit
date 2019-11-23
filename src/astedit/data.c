#define ASTEDIT_IMPLEMENT_DATA

#include <astedit/astedit.h>
#include <astedit/editor.h>
#include <astedit/clock.h>
#include <astedit/font.h>
#include <astedit/textedit.h>
#include <astedit/window.h>
#include <astedit/buffers.h>


const char *const vimodeKindString[NUM_VIMODE_KINDS] = {
        [VIMODE_NORMAL] = "NORMAL",
        [VIMODE_INPUT] = "INPUT",
        [VIMODE_SELECTING] = "SELECTING",
        [VIMODE_COMMAND] = "COMMAND",
};


const char *const inputKindString[NUM_INPUT_KINDS] = {
#define MAKE(x) [x] = #x
        MAKE( INPUT_KEY ),
        MAKE( INPUT_MOUSEBUTTON ),
        MAKE( INPUT_CURSORMOVE ),
        MAKE( INPUT_TIMETICK ),
        MAKE( INPUT_WINDOWRESIZE ),
        MAKE( INPUT_WINDOWEXPOSE ),
        MAKE( INPUT_WINDOWMAP ),
        MAKE( INPUT_WINDOWUNMAP ),
#undef MAKE
};
