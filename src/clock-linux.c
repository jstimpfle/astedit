#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <astedit/clock.h>
#include <errno.h>
#include <string.h>
#include <time.h>

void init_clock()
{
        //TODO
}

void update_clock(void)
{
        //TODO
}

void sleep_milliseconds(int ms)
{
        struct timespec ts;
        ts.tv_sec = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000000LL;

        struct timespec remain;

        int r;
        for (;;) {
                r = nanosleep(&ts, &remain);
                if (r != EINTR)
                        break;
        }
        if (r != 0)
                fatalf("nanosleep() failed: %s\n", strerror(errno));
}
