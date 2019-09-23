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

#include <astedit/logging.h>

static LARGE_INTEGER performanceFrequency;
static double microsecondsPerTick;

void setup_timers(void)
{
        QueryPerformanceFrequency(&performanceFrequency);
        microsecondsPerTick = 1000000.0 / performanceFrequency.QuadPart;
}

static uint64_t query_counter(void)
{
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return counter.QuadPart;
}

void start_timer(struct Timer *timer)
{
        timer->startTime = query_counter();
}

void stop_timer(struct Timer *timer)
{
        timer->stopTime = query_counter();
}

void report_timer(struct Timer *timer, const char *descriptionFmt, ...)
{
        log_begin();
        va_list ap;
        va_start(ap, descriptionFmt);
        log_writefv(descriptionFmt, ap);
        va_end(ap);
        log_writef(": %lld us", (long long)get_elapsed_microseconds(timer));
        log_end();
}

uint64_t get_elapsed_microseconds(struct Timer *timer)
{
        uint64_t diff = timer->stopTime - timer->startTime;
        return (uint64_t) (diff * microsecondsPerTick);
}