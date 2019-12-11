#include <astedit/logging.h>

void _alloc_memory(struct LogInfo logInfo, void **outPtr, int numElems, int elemSize);
void _realloc_memory(struct LogInfo logInfo, void **inoutPtr, int numElems, int elemSize);
void _free_memory(struct LogInfo logInfo, void **inoutPtr);

#define alloc_memory(outPtr, numElems, elemSize) _alloc_memory(MAKE_LOGINFO(), (outPtr), (numElems), (elemSize))
#define realloc_memory(inoutPtr, numElems, elemSize) _realloc_memory(MAKE_LOGINFO(), (inoutPtr), (numElems), (elemSize))
#define free_memory(inoutPtr) _free_memory(MAKE_LOGINFO(), (inoutPtr))

#define ALLOC_MEMORY(outPtr, numElems) alloc_memory((void**) (outPtr), (numElems), sizeof **(outPtr))
#define REALLOC_MEMORY(inoutPtr, numElems) realloc_memory((void**) (inoutPtr), (numElems), sizeof **(inoutPtr))
#define FREE_MEMORY(inoutPtr) free_memory((void**) (inoutPtr))
