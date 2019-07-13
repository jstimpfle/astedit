void alloc_memory(void **outPtr, int numElems, int elemSize);
void realloc_memory(void **inoutPtr, int numElems, int elemSize);
void free_memory(void **inoutPtr);

#define ALLOC_MEMORY(outPtr, numElems) alloc_memory((void**) (outPtr), (numElems), sizeof **(outPtr))
#define REALLOC_MEMORY(inoutPtr, numElems) realloc_memory((void**) (inoutPtr), (numElems), sizeof **(inoutPtr))
#define FREE_MEMORY(inoutPtr) free_memory((void**) (inoutPtr))
