#include <astedit/logging.h>
#include <astedit/memory.h>
#include <stdlib.h>

void realloc_memory(void **ptr, int numElems, int elemSize)
{
        int numBytes = numElems * elemSize + 16;
        void *p;
        if (*ptr)
                p = realloc((char *) *ptr - 16, numBytes);
        else
                p = malloc(numBytes);
        if (p == NULL)
                fatalf("OOM!\n");
        *(int *) p = numElems;
        *ptr = (char *) p + 16;
}

void alloc_memory(void **ptr, int numElems, int elemSize)
{
        *ptr = NULL;
        realloc_memory(ptr, numElems, elemSize);
}

void free_memory(void **ptr)
{
        free((char*)*ptr - 16);
        *ptr = NULL;
}
