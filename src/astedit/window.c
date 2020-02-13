#include <astedit/astedit.h>
#include <astedit/window.h>

static int numInputs;
static struct Input inpbuf[128];
static int inputFront;
static int inputBack;


void enqueue_input(const struct Input *inp)
{
        if (numInputs == LENGTH(inpbuf))
                return;
        inpbuf[inputBack] = *inp;
        inputBack = (inputBack + 1) % LENGTH(inpbuf);
        numInputs++;
}

void enqueue_key_input(
        enum KeyKind keyKind, enum KeyEventKind keyEventKind,
        int modifierMask, int hasCodepoint, uint32_t codepoint)
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

void enqueue_mousebutton_input(enum MousebuttonKind mousebuttonKind,
        enum MousebuttonEventKind mousebuttoneventKind,
        int modifiers)
{
        struct Input inp;
        inp.inputKind = INPUT_MOUSEBUTTON;
        inp.data.tMousebutton.mousebuttonKind = mousebuttonKind;
        inp.data.tMousebutton.mousebuttonEventKind = mousebuttoneventKind;
        inp.data.tMousebutton.modifiers = modifiers;
        enqueue_input(&inp);
}

void enqueue_cursormove_input(int xoff, int yoff)
{
        struct Input inp;
        inp.inputKind = INPUT_CURSORMOVE;
        inp.data.tCursormove.pixelX = (int)xoff;
        inp.data.tCursormove.pixelY = (int)yoff;
        enqueue_input(&inp);
}

void enqueue_windowsize_input(int width, int height)
{
        struct Input inp;
        inp.inputKind = INPUT_WINDOWRESIZE;
        inp.data.tWindowresize.width = width;
        inp.data.tWindowresize.height = height;
        enqueue_input(&inp);
}

int look_input(struct Input *out)
{
        if (numInputs == 0)
                return 0;
        *out = inpbuf[inputFront];
        return 1;
}

void consume_input(void)
{
        ENSURE(numInputs > 0);
        inputFront = (inputFront + 1) % LENGTH(inpbuf);
        numInputs--;
}
