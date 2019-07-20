#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <string.h>

void zero_memory(void *ptr, int numBytes)
{
        memset(ptr, 0, numBytes);
}

void copy_memory(void *dst, const void *src, int numBytes)
{
        memcpy(dst, src, numBytes);
}

void move_memory(void *ptr, int distance, int numBytes)
{
        memmove((char *) ptr + distance, ptr, numBytes);
}

void zero_array(void *ptr, int numElems, int elemSize)
{
        int numBytes = numElems * elemSize;  /* XXX: overflow */
        zero_memory(ptr, numBytes);
}

void copy_array(void *dst, const void *src, int numElems, int elemSize)
{
        int numBytes = numElems * elemSize;  /* XXX: overflow */
        copy_memory(dst, src, numBytes);
}
