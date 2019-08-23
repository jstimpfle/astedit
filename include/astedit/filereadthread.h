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

struct FilereadThreadHandle;

struct FilereadThreadHandle *run_file_read_thread(struct FilereadThreadCtx *ctx);
int check_if_file_read_thread_has_exited(struct FilereadThreadHandle *handle);
void wait_for_file_read_thread_to_end(struct FilereadThreadHandle *handle);
void dispose_file_read_thread(struct FilereadThreadHandle *handle);  // requires terminated thread

#endif