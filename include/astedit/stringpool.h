#ifndef STRINGPOOL_H_INCLUDED
#define STRINGPOOL_H_INCLUDED

enum {
        STRINGPOOLCHUNKSIZE_16,
        STRINGPOOLCHUNKSIZE_32,
        STRINGPOOLCHUNKSIZE_64,
        STRINGPOOLCHUNKSIZE_128,
        STRINGPOOLCHUNKSIZE_256,
        STRINGPOOLCHUNKSIZE_512,
        NUM_STRINGPOOLCHUNKSIZE_KINDS,
};


/* super shitty bookkeeping of allocations */
struct LinkedChunk {
        struct LinkedChunk *next;
        char data[];
};

struct StringPool {
        struct LinkedChunk *allocations;
        struct LinkedChunk *chunksList[NUM_STRINGPOOLCHUNKSIZE_KINDS];
};

void setup_stringpool(struct StringPool *pool);
void teardown_stringpool(struct StringPool *pool);
char *alloc_in_stringpool(struct StringPool *pool, const char *text, int length);

#endif
