#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/logging.h>
#include <astedit/stringpool.h>

static const int chunksizeKindToLength[NUM_STRINGPOOLCHUNKSIZE_KINDS] = {
        [STRINGPOOLCHUNKSIZE_16] = 16,
        [STRINGPOOLCHUNKSIZE_32] = 32,
        [STRINGPOOLCHUNKSIZE_64] = 64,
        [STRINGPOOLCHUNKSIZE_128] = 128,
        [STRINGPOOLCHUNKSIZE_256] = 256,
        [STRINGPOOLCHUNKSIZE_512] = 512,
};

void setup_stringpool(struct StringPool *pool)
{
        pool->allocations = NULL;
        for (int i = 0; i < NUM_STRINGPOOLCHUNKSIZE_KINDS; i++)
                pool->chunksList[i] = NULL;
}

void teardown_stringpool(struct StringPool *pool)
{
        struct LinkedChunk *chunk = pool->allocations;
        while (chunk) {
                struct LinkedChunk *next = chunk->next;
                FREE_MEMORY(chunk);
                chunk = next;
        }
        // for good hygiene
        ZERO_MEMORY(pool);
}

static void *allocate_in_pool(struct StringPool *pool, int numBytes)
{
        struct LinkedChunk *chunk;
        alloc_memory(&chunk, sizeof *chunk + numBytes, 1);
        chunk->next = pool->allocations;
        pool->allocations = chunk;
        return &chunk->data[0];  // alignment?
}

static void make_chunks(struct StringPool *pool, char *data, int remainBytes)
{
        /* make smaller chunks from unused space */
        for (int i = NUM_STRINGPOOLCHUNKSIZE_KINDS - 1; i >= 0; i--) {
                int thisLength = chunksizeKindToLength[i];
                ENSURE(remainBytes >= 0);
                if (remainBytes >= thisLength) {
                        /* add chunk */
                        struct LinkedChunk *unusedChunk = (void*)(data + remainBytes - thisLength);
                        unusedChunk->next = pool->chunksList[i];
                        pool->chunksList[i] = unusedChunk;
                        remainBytes -= thisLength;
                }
        }
}


char *alloc_in_stringpool(struct StringPool *pool, const char *text, int length)
{
        char *data = NULL;
        int chunksizeKind;

        /* try to find an existing StringPoolChunk of appropriate size */
        for (int i = 0; i < NUM_STRINGPOOLCHUNKSIZE_KINDS; i++) {
                if (chunksizeKindToLength[i] >= length + 1) {
                        if (pool->chunksList[i] != NULL) {
                                data = (void *)pool->chunksList[i];
                                chunksizeKind = i;
                                pool->chunksList[i] = pool->chunksList[i]->next;
                                break;
                        }
                }
        }

        /* no existing chunk found. Allocate chunk. Either of largest standard size or a huge-alloc */
        if (data == NULL) {
                chunksizeKind = NUM_STRINGPOOLCHUNKSIZE_KINDS - 1;
                int numBytes = chunksizeKindToLength[chunksizeKind];
                if (numBytes < length + 1) {
                        /* huge-alloc */
                        data = allocate_in_pool(pool, length + 1);
                }
                else {
                        /* normal alloc */
                        data = allocate_in_pool(pool, numBytes);
                        make_chunks(pool, data + length + 1, numBytes - length - 1);
                }
        }

        COPY_ARRAY(data, text, length);
        data[length] = '\0';
        return data;
}
