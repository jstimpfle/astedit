void realloc_memory(void **ptr, int numElems, int elemSize);
void alloc_memory(void **ptr, int numElems, int elemSize);
void free_memory(void **ptr);

enum {
        NUM_OVERALLOCATED_BYTES = 16
};

static inline int get_number_of_allocated_elems(void *ptr)
{
        if (!ptr)
                return 0;
        return *(int *) ((char *) ptr - NUM_OVERALLOCATED_BYTES);
}

static inline void realloc_memory_tophalf(void **ptr, int numElems, int elemSize)
{
        int cap = get_number_of_allocated_elems(*ptr);
        if (cap < numElems)
                realloc_memory(ptr, numElems, elemSize);
}

#define REALLOC_MEMORY(ptr, numElems) realloc_memory_tophalf((void**)(ptr), (numElems), sizeof **(ptr))
#define ALLOC_MEMORY(ptr, numElems) alloc_memory((void**)(ptr), (numElems), sizeof **(ptr))
#define FREE_MEMORY(ptr) free_memory((void**)(ptr))
