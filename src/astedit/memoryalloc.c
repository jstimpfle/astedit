#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <astedit/memoryalloc.h>
#include <stdlib.h>

void _alloc_memory(struct LogInfo logInfo, void **outPtr, int numElems, int elemSize)
{
        int numBytes = numElems * elemSize; /*XXX overflow*/
        void *ptr = malloc(numBytes);
        if (!ptr)
                _fatal(logInfo, "OOM!\n");
        *outPtr = ptr;
}


void _realloc_memory(struct LogInfo logInfo, void **inoutPtr, int numElems, int elemSize)
{
        int numBytes = numElems * elemSize; /*XXX overflow*/
        void *ptr = realloc(*inoutPtr, numBytes);
        if (!ptr)
                _fatal(logInfo, "OOM!\n");
        *inoutPtr = ptr;
}

void _free_memory(struct LogInfo logInfo, void **inoutPtr)
{
        free(*inoutPtr);
        *inoutPtr = NULL;
}
