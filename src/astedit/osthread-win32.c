#include <astedit/astedit.h>
#include <astedit/memoryalloc.h>
#include <astedit/logging.h>
#include <astedit/osthread.h>
#include <Windows.h>

struct OsThreadCtx {
        OsThreadEntryFunc *entryFunc;
        void *param;
};

struct OsThreadHandle {
        HANDLE win32Handle;
        struct OsThreadCtx ctx;
};

static DWORD WINAPI thread_adapter_for_win32(LPVOID param)
{
        struct OsThreadCtx *ctx = param;
        (*ctx->entryFunc)(ctx->param);
        return 0;
}

struct OsThreadHandle *create_and_start_thread(OsThreadEntryFunc *entryFunc, void *param)
{
        struct OsThreadHandle *handle;
        ALLOC_MEMORY(&handle, 1);
        handle->ctx.entryFunc = entryFunc;
        handle->ctx.param = param;
        //TODO https://stackoverflow.com/questions/58019610/is-createthread-a-synchronization-point
        handle->win32Handle = CreateThread(NULL, 0, thread_adapter_for_win32, &handle->ctx, 0, NULL);
        if (handle->win32Handle == NULL)
                fatalf("Failed to create thread\n");
        return handle;
}

int check_if_thread_has_exited(struct OsThreadHandle *handle)
{
        DWORD exitCode;
        BOOL ret = GetExitCodeThread(handle->win32Handle, &exitCode);
        ENSURE(ret != 0);
        UNUSED(ret);
        return exitCode != STILL_ACTIVE;
}

void wait_for_thread_to_end(struct OsThreadHandle *handle)
{
        WaitForSingleObject(handle->win32Handle, INFINITE);
}

void cancel_thread_and_wait(struct OsThreadHandle *handle)
{
        // TODO: check docs if this is really synchronous
        BOOL ret = TerminateThread(handle->win32Handle, 1);
        ENSURE(ret != 0);
        UNUSED(ret);
}

void dispose_thread(struct OsThreadHandle *handle)
{
        ENSURE(check_if_thread_has_exited(handle));
        BOOL ret = CloseHandle(handle->win32Handle);
        ENSURE(ret != 0);
        UNUSED(ret);
        FREE_MEMORY(&handle);
}
