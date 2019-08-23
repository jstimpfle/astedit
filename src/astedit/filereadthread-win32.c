#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/logging.h>
#include <astedit/window.h>  // shouldWindowClose
#include <astedit/filereadthread.h>
#include <stdio.h>
#include <Windows.h>


struct FilereadThreadHandle {
        HANDLE threadHandle;
};

#include <sys/stat.h>
static void read_file_thread(struct FilereadThreadCtx *ctx)
{
        int returnStatus = 0;

        FILE *f = fopen(ctx->filepath, "rb");
        if (!f) {
                log_postf("Failed to open file %s\n", ctx->filepath);
                returnStatus = -1;
                goto out1;
        }

        {
                struct _stati64 _buf;
                int result = _fstati64(_fileno(f), &_buf);
                if (result != 0) {
                        log_postf("Error determining file size\n");
                        returnStatus = -1;
                        goto out2;
                }
                if (_buf.st_size > FILEPOS_MAX - 1) {
                        log_postf("File too large!\n");
                        returnStatus = -1;
                        goto out2;
                }
                FILEPOS filesize = (FILEPOS) _buf.st_size;
                ENSURE(filesize == _buf.st_size); // dirty (and probably technically invalid) way to check cast
                ctx->prepareFunc(ctx->param, filesize);
        }

        for (;;) {
                if (shouldWindowClose) {
                        /* Thread should terminate. We will abort the loading.
                        We might want a more general inter-thread communication system.
                        For now, this variable is good enough (although technically we
                        need to protect the access to make sure that we get the new value
                        if it was updated from another thread). */
                        returnStatus = -1;  // TODO: choose a different return code? This is not strictly an error.
                        goto out3;
                }

                size_t n = fread(ctx->buffer + *ctx->bufferFill, 1,
                                        ctx->bufferSize - *ctx->bufferFill, f);

                if (n == 0)
                        /* EOF. Ignore remaining undecoded bytes */
                        break;

                *ctx->bufferFill += (int) n;

                int r = (*ctx->flushBufferFunc)(ctx->param);

                if (r == -1) {
                        returnStatus = -1;
                        goto out3;
                }
        }

        if (ferror(f)) {
                log_postf("Errors while reading from file %s\n", ctx->filepath);
                returnStatus = -1;
        }

out3:
        ctx->finalizeFunc(ctx->param);
out2:
        fclose(f);
out1:
        ctx->returnStatus = returnStatus;
}

static DWORD WINAPI read_file_thread_adapter(LPVOID param)
{
        struct FilereadThreadCtx *ctx = param;
        read_file_thread(ctx);
        return ctx->returnStatus;
}


struct FilereadThreadHandle *run_file_read_thread(struct FilereadThreadCtx *ctx)
{
        struct FilereadThreadHandle *handle;
        ALLOC_MEMORY(&handle, 1);
        handle->threadHandle = CreateThread(NULL, 0, &read_file_thread_adapter, ctx, 0, NULL);
        if (handle->threadHandle == NULL)
                fatalf("Failed to create thread\n");

        return handle;
}

int check_if_file_read_thread_has_exited(struct FilereadThreadHandle *handle)
{
        DWORD exitCode;
        BOOL ret = GetExitCodeThread(handle->threadHandle, &exitCode);
        ENSURE(ret != 0);
        UNUSED(ret);
        return exitCode != STILL_ACTIVE;
}

void wait_for_file_read_thread_to_end(struct FilereadThreadHandle *handle)
{
        WaitForSingleObject(handle->threadHandle, INFINITE);
}

void dispose_file_read_thread(struct FilereadThreadHandle *handle)
{
        ENSURE(check_if_file_read_thread_has_exited(handle));
        BOOL ret = CloseHandle(handle->threadHandle);
        ENSURE(ret != 0);
        UNUSED(ret);
        FREE_MEMORY(&handle);
}
