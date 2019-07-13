#include <astedit/astedit.h>
#include <astedit/bytes.h>

void zero_memory(void *ptr, int numBytes)
{
        unsigned char *x = ptr;
        while (numBytes--)
                *x++ = 0;
}

void zero_array(void *ptr, int numElems, int elemSize)
{
        int numBytes = numElems * elemSize;  /* XXX: overflow */
        zero_memory(ptr, numBytes);
}


void copy_memory(void *dst, const void *src, int numBytes)
{
        unsigned char *x = dst;
        const unsigned char *y = src;
        while (numBytes--)
                *x++ = *y++;
}

void copy_array(void *dst, const void *src, int numElems, int elemSize)
{
        int numBytes = numElems * elemSize;  /* XXX: overflow */
        copy_memory(dst, src, numBytes);
}