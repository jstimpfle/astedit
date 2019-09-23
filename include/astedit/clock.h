#ifndef ASTEDIT_CLOCK_H_INCLUDED
#define ASTEDIT_CLOCK_H_INCLUDED

#include <astedit/astedit.h>

/* Define struct Timer by including platform specific headers */
#ifdef _MSVC  // TODO: better defined detection macros?
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
void report_timer(struct Timer *timer, const char *descriptionFmt, ...);
uint64_t get_elapsed_microseconds(struct Timer *timer);


#endif
