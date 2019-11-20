#ifndef ASTEDIT_CLOCK_H_INCLUDED
#define ASTEDIT_CLOCK_H_INCLUDED

#include <astedit/astedit.h>
#include <astedit/logging.h>

/* Define struct Timer by including platform specific headers */
#ifdef _MSC_VER  // TODO: better defined detection macros?
#include <astedit/clock-win32.h>
#else
#include <astedit/clock-linux.h>
#endif

DATA long long timeSinceProgramStartupMilliseconds;

/* clock-XXX.c */
void init_clock(void);
void update_clock(void);
void sleep_milliseconds(int ms);


void setup_timers(void);

void start_timer(struct Timer *timer);
void stop_timer(struct Timer *timer);
uint64_t get_elapsed_microseconds(struct Timer *timer);

void _report_timer(struct LogInfo logInfo, struct Timer *timer, const char *descriptionFmt, ...);
#define report_timer(...) _report_timer(MAKE_LOGINFO(), ##__VA_ARGS__)


#endif
