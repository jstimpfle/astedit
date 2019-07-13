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
