#include <astedit/astedit.h>
#include <astedit/memoryalloc.h>
#include <astedit/logging.h>
#include <astedit/osthread.h>
#include <Windows.h>

struct OsThreadHandle {
        HANDLE threadHandle;
        OsThreadEntryFunc *entryFunc;
        void *param;
};

static DWORD WINAPI thread_adapter_for_win32(LPVOID param)
{
        struct OsThreadHandle *handle = param;
        (*handle->entryFunc)(handle->param);
        return 0;
}

struct OsThreadHandle *create_and_start_thread(OsThreadEntryFunc *entryFunc, void *param)
{
        //XXX not nice
        struct OsThreadHandle *handle;
        ALLOC_MEMORY(&handle, 1);
        handle->entryFunc = entryFunc;
        handle->param = param;
        //XXX we might need a memory barrier here, to guarantee that entryFunc and param
        //are already written before the thread tries to access them.
        HANDLE threadHandle = CreateThread(NULL, 0, thread_adapter_for_win32, handle, 0, NULL);
        if (threadHandle == NULL)
                fatalf("Failed to create thread\n");
        handle->threadHandle = threadHandle;
        return handle;
}

int check_if_thread_has_exited(struct OsThreadHandle *handle)
{
        DWORD exitCode;
        BOOL ret = GetExitCodeThread(handle->threadHandle, &exitCode);
        ENSURE(ret != 0);
        UNUSED(ret);
        return exitCode != STILL_ACTIVE;
}

void wait_for_thread_to_end(struct OsThreadHandle *handle)
{
        WaitForSingleObject(handle->threadHandle, INFINITE);
}

void dispose_thread(struct OsThreadHandle *handle)
{
        ENSURE(check_if_thread_has_exited(handle));
        BOOL ret = CloseHandle(handle->threadHandle);
        ENSURE(ret != 0);
        UNUSED(ret);
        FREE_MEMORY(&handle);
}
