#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <astedit/clock.h>
#include <astedit/memoryalloc.h>
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
                if (r == 0)
                        break;
                ENSURE(r == -1);
                if (errno != EINTR)
                        fatalf("nanosleep() failed: %s\n", strerror(errno));
                ts = remain;
        }
}

static void get_time(struct timespec *ts)
{
        int r = clock_gettime(CLOCK_MONOTONIC, ts);
        if (r == -1)
                fatalf("clock_gettime() failed: %s\n", strerror(errno));
}






void setup_timers(void)
{
}

void start_timer(struct Timer *timer)
{
        get_time(&timer->startTime);
}


void stop_timer(struct Timer *timer)
{
        get_time(&timer->stopTime);
}

void _report_timer(struct LogInfo logInfo, struct Timer *timer, const char *descriptionFmt, ...)
{
        va_list ap;
        va_start(ap, descriptionFmt);
        _log_begin(logInfo);
        log_writefv(descriptionFmt, ap);
        log_writef(": %lld us", (long long)get_elapsed_microseconds(timer));
        log_end();
        va_end(ap);
}

uint64_t get_elapsed_microseconds(struct Timer *timer)
{
        int64_t diff = ((int64_t) timer->stopTime.tv_sec - (int64_t) timer->startTime.tv_sec) * (int64_t) 1000000
            + ((int64_t) timer->stopTime.tv_nsec - (int64_t) timer->startTime.tv_nsec) / (int64_t) 1000;
        ENSURE(diff >= 0);
        return (uint64_t) diff;
}
