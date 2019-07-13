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
