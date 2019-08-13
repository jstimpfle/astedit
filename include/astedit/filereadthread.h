#ifndef FILEREADTHREAD_H_INCLUDED
#define FILEREADTHREAD_H_INCLUDED

struct FilereadThreadCtx;

struct FilereadThreadCtx *run_file_read_thread(
        const char *filepath, void *param,
        char *buffer, int bufferSize, int *bufferFill,
        void(*prepare)(void *param, int filesizeInBytes),
        void(*finalize)(void *param),
        int(*flush_buffer)(void *param));
int check_if_file_read_thread_has_exited(struct FilereadThreadCtx *ctx);
void wait_for_file_read_thread_to_end(struct FilereadThreadCtx *ctx);
void dispose_file_read_thread(struct FilereadThreadCtx *ctx);  // requires terminated thread

#endif