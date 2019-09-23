#ifndef ASTEDIT_CLOCK_WIN32_H_INCLUDED
#define ASTEDIT_CLOCK_WIN32_H_INCLUDED

#ifndef ASTEDIT_ASTEDIT_H_INCLUDED
#include <astedit/astedit.h>
#endif
#ifndef ASTEDIT_CLOCK_H_INCLUDED
#include <astedit/clock.h>
#endif

#include <time.h>  // should we hide this and not use struct timespec in the interface?

struct Timer {
        struct timespec startTime;
        struct timespec stopTime;
};

#endif
