#define ASTEDIT_IMPLEMENT_DATA

#include <astedit/astedit.h>
#include <astedit/window.h>
#include <astedit/clock.h>
#include <astedit/textedit.h>


const char *const vimodeKindString[NUM_VIMODE_KINDS] = {
        [VIMODE_NORMAL] = "NORMAL",
        [VIMODE_INPUT] = "INPUT",
        [VIMODE_SELECTING] = "SELECTING",
};
