#ifndef ASTEDIT_ASTEDIT_H_INCLUDED
#define ASTEDIT_ASTEDIT_H_INCLUDED

#include <stdarg.h>
#include <assert.h>


#ifdef _MSC_VER
#define NORETURN __declspec(noreturn)
#else  // assume gcc or clang
#define NORETURN __attribute__((noreturn))
#endif

#define LENGTH(a) (sizeof (a) / sizeof (a)[0])
#define ENSURE(a) assert(a)

static inline NORETURN void UNREACHABLE(void)
{
        ENSURE(0);
}

#ifdef ASTEDIT_IMPLEMENT_DATA
#define DATA
#else
#define DATA extern
#endif

#endif