#ifndef ASTEDIT_ASTEDIT_H_INCLUDED
#define ASTEDIT_ASTEDIT_H_INCLUDED


#ifdef _MSC_VER
#define NORETURN __declspec(noreturn)
#endif

#define LENGTH(a) (sizeof (a) / sizeof (a)[0])
#define ENSURE(a) assert(a)

#ifdef ASTEDIT_IMPLEMENT_DATA
#define DATA
#else
#define DATA extern
#endif

#include <stdarg.h>
#include <assert.h>

#endif