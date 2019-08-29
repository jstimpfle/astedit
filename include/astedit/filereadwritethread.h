#ifndef FILEREADTHREAD_H_INCLUDED
#define FILEREADTHREAD_H_INCLUDED

#include <astedit/filepositions.h>  // FILEPOS

struct FilereadThreadCtx {
        char *filepath;

        /* Read buffer provided by the caller to the reader thread. */
        char *buffer;
        int bufferSize;
        int *bufferFill;

        /* Context provided by the caller to the reader thread.
        Reader thread hands param to caller when calling back. */
        void *param;

        /* Callbacks */
        void(*prepareFunc)(void *param, FILEPOS filesizeInBytes);
        void(*finalizeFunc)(void *param);
        /* to flush the buffer completely. Return value is 0 in case of success,
        or -1 in case of error (=> reader thread should terminate itself) */
        int (*flushBufferFunc)(void *param);

        /* thread can report return status here, so we don't depend on OS facilities
        which might be hard to use or have their own gotchas... */
        int returnStatus;
};


void read_file_thread(struct FilereadThreadCtx *ctx);
void read_file_thread_adapter(void *param);







struct FilewriteThreadCtx {
        char *filepath;  // write destination.

        /* Write buffer provided by the caller to the writer thread. */
        char *buffer;
        int bufferSize;
        int *bufferFill;

        /* Context provided by the caller to the reader thread.
        Reader thread hands param to caller when calling back. */
        void *param;

        /* Callbacks */
        void (*prepareFunc)(void *param);
        void (*finalizeFunc)(void *param);
        /*When the function returns, *bufferFill is updated. */
        void (*fillBufferFunc)(void *param);

        /* thread can report return status here, so we don't depend on OS facilities
        which might be hard to use or have their own gotchas... */
        int returnStatus;
};


void write_file_thread(struct FilewriteThreadCtx *ctx);
void write_file_thread_adapter(void *param);


#endif
