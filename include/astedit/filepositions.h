#ifndef ASTEDIT_FILEPOSITIONS_H_INCLUDED
#define ASTEDIT_FILEPOSITIONS_H_INCLUDED

#ifndef ASTEDIT_ASTEDIT_H_INCLUDED
#include <astedit/astedit.h>
#endif

#ifndef INT_MIN  // detect header
#include <limits.h>
#endif


/*
This type is currently used both for file sizes and offsets,
and differences thereof.
*/


typedef int64_t FILEPOS;
#define FILEPOS_MIN INT64_MIN
#define FILEPOS_MAX INT64_MAX
#define FILEPOS_PRI PRId64

static inline FILEPOS filepos_add(FILEPOS a, FILEPOS b)
{
        ENSURE(a + b >= 0);  // crude and technically invalid way of checking overflow
        return a + b;
}

static inline FILEPOS filepos_sub(FILEPOS a, FILEPOS b)
{
        ENSURE(a - b >= 0);  // crude and technically invalid way of checking overflow
        return a - b;
}

static inline FILEPOS filepos_mul(FILEPOS a, int factor)
{
        ENSURE(factor >= 0);
        ENSURE(a * factor >= 0);  // crude and technically invalid way of checking overflow
        return a * factor;
}

static inline int cast_filepos_to_int(FILEPOS a)
{
        ENSURE(a >= INT_MIN);
        ENSURE(a <= INT_MAX);
        return (int) a;
}

/* this is useful when one of the operands is actually an integer */
static inline int min_filepos_as_int(FILEPOS a, FILEPOS b)
{
        return cast_filepos_to_int(a < b ? a : b);
}


#endif
