#ifndef FILEREADTHREAD_H_INCLUDED
#define FILEREADTHREAD_H_INCLUDED

#include <astedit/filepositions.h>  // FILEPOS

struct FilereadThreadHandle;

struct FilereadThreadHandle *run_file_read_thread(
        const char *filepath, void *param,
        char *buffer, int bufferSize, int *bufferFill,
        void(*prepareFunc)(void *param, FILEPOS filesizeInBytes),
        void(*finalizeFunc)(void *param),
        int(*flushBufferFunc)(void *param));
int check_if_file_read_thread_has_exited(struct FilereadThreadHandle *handle);
void wait_for_file_read_thread_to_end(struct FilereadThreadHandle *handle);
void dispose_file_read_thread(struct FilereadThreadHandle *handle);  // requires terminated thread

#endif