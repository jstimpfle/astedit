#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/logging.h>
#include <stdio.h>
#include <Windows.h>

struct FilereadThreadCtx {
        char *filepath;
        /* thread handle. */
        HANDLE handle;
        /* additional user value that is handed to the flush_buffer() function below */
        void *param;

        void(*prepare)(void *param);
        void(*finalize)(void *param);
        /* to flush the buffer completely. Return values is 0 for success
        or -1 to indicate error (reader thread should terminate itself in this case */
        int (*flush_buffer)(const char *buffer, int length, void *param);
        /* thread can report return status here, so we don't depend on OS facilities
        which might be hard to use or have their own gotchas... */
        int returnStatus;
};

static void read_file_thread(struct FilereadThreadCtx *ctx)
{
        ctx->prepare(ctx->param);

        FILE *f = fopen(ctx->filepath, "rb");
        if (!f)
                fatalf("Failed to open file %s\n", ctx->filepath);

        char buf[4096];

        for (;;) {
                size_t n = fread(buf, 1, sizeof buf, f);

                if (n == 0)
                        /* EOF. Ignore remaining undecoded bytes */
                        break;

                int r = (*ctx->flush_buffer)(buf, (int) n, ctx->param);

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
        void (*prepare)(void *param),
        void (*finalize)(void *param),
        int (*flush_buffer)(const char *buffer, int length, void *param))
{
        struct FilereadThreadCtx *ctx;
        ALLOC_MEMORY(&ctx, 1);

        {
                int filepathLen = (int) strlen(filepath);
                ALLOC_MEMORY(&ctx->filepath, filepathLen + 1);
                COPY_ARRAY(ctx->filepath, filepath, filepathLen + 1);
        }

        ctx->param = param;
        ctx->prepare = prepare;
        ctx->finalize = finalize;
        ctx->flush_buffer = flush_buffer;

        ctx->handle = CreateThread(NULL, 0, &read_file_thread_adapter, ctx, 0, NULL);
        if (ctx->handle == NULL)
                fatalf("Failed to create thread\n");

        return ctx;
}

int check_if_file_read_thread_has_exited(struct FilereadThreadCtx *ctx)
{
        DWORD exitCode;
        BOOL ret = GetExitCodeThread(ctx->handle, &exitCode);
        ENSURE(ret != 0);
        UNUSED(ret);
        return exitCode != STILL_ACTIVE;
}

void dispose_file_read_thread(struct FilereadThreadCtx *ctx)
{
        ENSURE(check_if_file_read_thread_has_exited(ctx));  // for now
        BOOL ret = CloseHandle(ctx->handle);
        ENSURE(ret != 0);
        UNUSED(ret);

        FREE_MEMORY(ctx->filepath);
        FREE_MEMORY(ctx);
}
