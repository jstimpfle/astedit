#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/logging.h>
#include <astedit/window.h>  // shouldWindowClose
#include <astedit/filereadthread.h>
#include <stdio.h>
#include <Windows.h>

struct FilereadThreadCtx {
        char *filepath;
        HANDLE threadHandle;
        /* additional user value that is handed to the flush_buffer() function below */
        void *param;

        char *buffer;
        int bufferSize;
        int *bufferFill;

        void(*prepare)(void *param, FILEPOS filesizeInBytes);
        void(*finalize)(void *param);
        /* to flush the buffer completely. Return values is 0 for success
        or -1 to indicate error (reader thread should terminate itself in this case */
        int (*flush_buffer)(void *param);
        /* thread can report return status here, so we don't depend on OS facilities
        which might be hard to use or have their own gotchas... */
        int returnStatus;
};

#include <sys/stat.h>
static void read_file_thread(struct FilereadThreadCtx *ctx)
{
        int returnStatus = 0;

        FILE *f = fopen(ctx->filepath, "rb");
        if (!f) {
                log_postf("Failed to open file %s\n", ctx->filepath);
                returnStatus = -1;
                goto out;
        }

        {
                struct _stati64 _buf;
                int result = _fstati64(_fileno(f), &_buf);
                if (result != 0) {
                        log_postf("Error determining file size\n");
                        returnStatus = -1;
                        goto out;
                }
                if (_buf.st_size > FILEPOS_MAX - 1) {
                        log_postf("File too large!\n");
                        returnStatus = -1;
                }
                FILEPOS filesize = (FILEPOS) _buf.st_size;
                ENSURE(filesize == _buf.st_size); // dirty (and probably technically invalid) way to check cast
                ctx->prepare(ctx->param, filesize);
        }

        for (;;) {
                if (shouldWindowClose) {
                        /* Thread should terminate. We will abort the loading.
                        We might want a more general inter-thread communication system.
                        For now, this variable is good enough (although technically we
                        need to protect the access to make sure that we get the new value
                        if it was updated from another thread). */
                        returnStatus = -1;  // TODO: choose a different return code? This is not strictly an error.
                        goto out;
                }

                int bufferSize = ctx->bufferSize;
                int bufferFill = *ctx->bufferFill;
                size_t n = fread(ctx->buffer + bufferFill, 1, bufferSize - bufferFill, f);

                if (n == 0)
                        /* EOF. Ignore remaining undecoded bytes */
                        break;

                bufferFill += (int) n;
                *ctx->bufferFill = bufferFill;

                int r = (*ctx->flush_buffer)(ctx->param);

                if (r == -1) {
                        returnStatus = -1;
                        goto out;
                }
        }

        if (ferror(f)) {
                log_postf("Errors while reading from file %s\n", ctx->filepath);
                returnStatus = -1;
                goto out;
        }

out:
        if (f != NULL)
                fclose(f);
        ctx->returnStatus = returnStatus;
        ctx->finalize(ctx->param);
}

static DWORD WINAPI read_file_thread_adapter(LPVOID param)
{
        struct FilereadThreadCtx *ctx = param;
        read_file_thread(ctx);
        return ctx->returnStatus;
}


struct FilereadThreadCtx *run_file_read_thread(
        const char *filepath, void *param,
        char *buffer, int bufferSize, int *bufferFill,
        void (*prepare)(void *param, FILEPOS filesizeInBytes),
        void (*finalize)(void *param),
        int (*flush_buffer)(void *param))
{
        struct FilereadThreadCtx *ctx;
        ALLOC_MEMORY(&ctx, 1);

        {
                int filepathLen = (int) strlen(filepath);
                ALLOC_MEMORY(&ctx->filepath, filepathLen + 1);
                COPY_ARRAY(ctx->filepath, filepath, filepathLen + 1);
        }

        ctx->param = param;
        ctx->buffer = buffer;
        ctx->bufferSize = bufferSize;
        ctx->bufferFill = bufferFill;
        ctx->prepare = prepare;
        ctx->finalize = finalize;
        ctx->flush_buffer = flush_buffer;

        ctx->threadHandle = CreateThread(NULL, 0, &read_file_thread_adapter, ctx, 0, NULL);
        if (ctx->threadHandle == NULL)
                fatalf("Failed to create thread\n");

        return ctx;
}

int check_if_file_read_thread_has_exited(struct FilereadThreadCtx *ctx)
{
        DWORD exitCode;
        BOOL ret = GetExitCodeThread(ctx->threadHandle, &exitCode);
        ENSURE(ret != 0);
        UNUSED(ret);
        return exitCode != STILL_ACTIVE;
}

void wait_for_file_read_thread_to_end(struct FilereadThreadCtx *ctx)
{
        WaitForSingleObject(ctx->threadHandle, INFINITE);
}

void dispose_file_read_thread(struct FilereadThreadCtx *ctx)
{
        ENSURE(check_if_file_read_thread_has_exited(ctx));
        BOOL ret = CloseHandle(ctx->threadHandle);
        ENSURE(ret != 0);
        UNUSED(ret);
        FREE_MEMORY(&ctx->filepath);
        FREE_MEMORY(&ctx);
}
