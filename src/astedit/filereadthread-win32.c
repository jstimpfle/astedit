#include <astedit/astedit.h>
#include <astedit/memoryalloc.h>
#include <astedit/logging.h>
#include <astedit/filereadthread.h>
#include <Windows.h>

struct FilereadThreadHandle {
        HANDLE threadHandle;
};

static DWORD WINAPI read_file_thread_adapter(LPVOID param)
{
        struct FilereadThreadCtx *ctx = param;
        read_file_thread(ctx);
        return ctx->returnStatus;
}

struct FilereadThreadHandle *run_file_read_thread(struct FilereadThreadCtx *ctx)
{
        HANDLE threadHandle = CreateThread(NULL, 0, &read_file_thread_adapter, ctx, 0, NULL);
        if (threadHandle == NULL)
                fatalf("Failed to create thread\n");
        struct FilereadThreadHandle *handle;
        ALLOC_MEMORY(&handle, 1);
        handle->threadHandle = threadHandle;
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
