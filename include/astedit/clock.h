#ifndef ASTEDIT_CLOCK_H_INCLUDED
#define ASTEDIT_CLOCK_H_INCLUDED

DATA long long timeSinceProgramStartupMilliseconds;

/* clock-XXX.c */
void init_clock(void);
void update_clock(void);
void sleep_milliseconds(int ms);

#endif