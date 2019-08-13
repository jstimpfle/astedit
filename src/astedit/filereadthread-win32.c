#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/logging.h>
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

        void(*prepare)(void *param, int filesizeInBytes);
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
        FILE *f = fopen(ctx->filepath, "rb");
        if (!f)
                fatalf("Failed to open file %s\n", ctx->filepath);

        {
                struct _stat _buf;
                int result = _fstat(_fileno(f), &_buf);
                if (result != 0)
                        fatalf("Error determining file size\n");
                int filesize = _buf.st_size;
                ctx->prepare(ctx->param, filesize);
        }

        for (;;) {
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
                        /* what to report? */
                        ctx->returnStatus = -1;
                        break;
                }
        }

        if (ferror(f))
                fatalf("Errors while reading from file %s\n", ctx->filepath);
        fclose(f);

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
        void (*prepare)(void *param, int filesizeInBytes),
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
