#ifndef FILEREADTHREAD_H_INCLUDED
#define FILEREADTHREAD_H_INCLUDED

struct FilereadThreadCtx;

struct FilereadThreadCtx *run_file_read_thread(
        const char *filepath, void *param,
        void(*prepare)(void *param, int filesizeInBytes),
        void(*finalize)(void *param),
        int(*flush_buffer)(const char *buffer, int length, void *param));
int check_if_file_read_thread_has_exited(struct FilereadThreadCtx *ctx);
void dispose_file_read_thread(struct FilereadThreadCtx *ctx);

#endif