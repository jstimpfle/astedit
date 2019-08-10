#ifndef ASTEDIT_CLOCK_H_INCLUDED
#define ASTEDIT_CLOCK_H_INCLUDED

#include <astedit/astedit.h>

DATA long long timeSinceProgramStartupMilliseconds;

/* clock-XXX.c */
void init_clock(void);
void update_clock(void);
void sleep_milliseconds(int ms);


typedef struct TimerStruct Timer;

Timer *create_timer(void);
void destroy_timer(Timer *timer);

void start_timer(Timer *timer);
void stop_timer(Timer *timer);
void report_timer(Timer *timer, const char *description);
uint64_t get_elapsed_microseconds(Timer *timer);

#endif