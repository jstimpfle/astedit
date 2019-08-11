#include <astedit/astedit.h>
#include <astedit/clock.h>

#include <Windows.h>

DWORD timeOfLastUpdateMs;

void init_clock()
{
        timeOfLastUpdateMs = timeGetTime();
}

void update_clock(void)
{
        DWORD newTimeMs = timeGetTime();
        long long timeDeltaMs = newTimeMs - timeOfLastUpdateMs;
        if (timeDeltaMs < 0)
                timeDeltaMs += 1LL << 32;
        timeSinceProgramStartupMilliseconds += timeDeltaMs;
        timeOfLastUpdateMs = newTimeMs;
}

void sleep_milliseconds(int ms)
{
        Sleep(ms);
}






struct TimerStruct {
        LARGE_INTEGER startTime;
        LARGE_INTEGER stopTime;
};

#include <astedit/memoryalloc.h>
#include <astedit/logging.h>

static LARGE_INTEGER performanceFrequency;
static double microsecondsPerTick;

Timer *create_timer(void)
{
        // TODO: do this only at program startup.
        QueryPerformanceFrequency(&performanceFrequency);
        microsecondsPerTick = 1000000.0 / performanceFrequency.QuadPart;

        Timer *timer;
        ALLOC_MEMORY(&timer, 1);
        timer->startTime.QuadPart = 0;
        timer->stopTime.QuadPart = 0;
        return timer;
}

void destroy_timer(Timer *timer)
{
        FREE_MEMORY(&timer);
}

void start_timer(Timer *timer)
{
        QueryPerformanceCounter(&timer->startTime);
}

void stop_timer(Timer *timer)
{
        QueryPerformanceCounter(&timer->stopTime);
}

void report_timer(Timer *timer, const char *descriptionFmt, ...)
{
        va_list ap;
        va_start(ap, descriptionFmt);
        log_writefv(descriptionFmt, ap);
        va_end(ap);

        log_begin();
        log_writef(": %lld us", (long long)get_elapsed_microseconds(timer));
        log_end();
}

uint64_t get_elapsed_microseconds(Timer *timer)
{
        uint64_t diff = timer->stopTime.QuadPart - timer->startTime.QuadPart;
        return (uint64_t) (diff * microsecondsPerTick);
}