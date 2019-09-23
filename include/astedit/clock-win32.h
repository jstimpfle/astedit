#ifndef ASTEDIT_CLOCK_WIN32_H_INCLUDED
#define ASTEDIT_CLOCK_WIN32_H_INCLUDED

#ifndef ASTEDIT_ASTEDIT_H_INCLUDED
#include <astedit/astedit.h>
#endif
#ifndef ASTEDIT_CLOCK_H_INCLUDED
#include <astedit/clock.h>
#endif

struct Timer {
        /* We're using uint64_t instead of LARGE_INTEGER (as required to call
        QueryPerformanceCounter) here, simply to avoid including Windows.h */
        /* these field still count ticks, so are not meaningful to callers
        of the timer API. Use get_elapsed_microseconds(). */
        uint64_t startTime;
        uint64_t stopTime;
};

#endif