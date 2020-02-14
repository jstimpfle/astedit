#include <astedit/logging.h>
#include <astedit/memory.h>
#include <stdlib.h>


void realloc_memory(void **ptr, int numElems, int elemSize)
{
        int numAlloced = get_number_of_allocated_elems(*ptr);
        if (numAlloced < numElems) {
                if (numAlloced == 0)
                        numAlloced = numElems;
                else {
                        numAlloced = 2 * numAlloced;
                        if (numAlloced < numElems)
                                numAlloced = numElems;
                }
        }
        int numBytes = numAlloced * elemSize + NUM_OVERALLOCATED_BYTES;
        void *p;
        if (*ptr)
                p = realloc((char *) *ptr - NUM_OVERALLOCATED_BYTES, numBytes);
        else
                p = malloc(numBytes);
        if (p == NULL)
                fatalf("OOM!\n");
        *(int *) p = numAlloced;
        *ptr = (char *) p + NUM_OVERALLOCATED_BYTES;
}

void alloc_memory(void **ptr, int numElems, int elemSize)
{
        *ptr = NULL;
        realloc_memory(ptr, numElems, elemSize);
}

void free_memory(void **ptr)
{
        if (*ptr != NULL)
                free((char*)*ptr - NUM_OVERALLOCATED_BYTES);
        *ptr = NULL;
}
