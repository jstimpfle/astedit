#ifndef ASTEDIT_ASTEDIT_H_INCLUDED
#define ASTEDIT_ASTEDIT_H_INCLUDED

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>  // NULL
#include <stdint.h>  // uint32_t and friends


#ifdef _MSC_VER
#define NORETURN __declspec(noreturn)
#define UNUSEDFUNC
#else  // assume gcc or clang
#define NORETURN __attribute__((noreturn))
#define UNUSEDFUNC __attribute__((unused))
#endif

#define LENGTH(a) (sizeof (a) / sizeof (a)[0])
#define ENSURE(a) assert(a)
#define UNUSED(arg) (void)(arg)

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
