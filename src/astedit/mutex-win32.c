#include <astedit/astedit.h>
#include <astedit/memoryalloc.h>
#include <astedit/logging.h>
#include <astedit/mutex.h>
#include <Windows.h>

struct Mutex {
        HANDLE handle;
};

struct Mutex *create_mutex(void)
{
        HANDLE handle = CreateMutex(NULL, FALSE, NULL);
        if (handle == NULL)
                fatal("Failed to CreateMutex()");
        struct Mutex *mutex;
        ALLOC_MEMORY(&mutex, 1);
        mutex->handle = handle;
        return mutex;
}

void destroy_mutex(struct Mutex *mutex)
{
        BOOL ret = CloseHandle(mutex->handle);
        if (ret == 0)
                fatal("Error detected in CloseHandle() on mutex handle");
        FREE_MEMORY(&mutex);
}

void lock_mutex(struct Mutex *mutex)
{
        DWORD waitResult = WaitForSingleObject(mutex->handle, INFINITE);
        if (waitResult != WAIT_OBJECT_0)
                fatalf("Failed to take mutex using WaitForSingleObject(). "
                        "Result value is %d\n", (int) waitResult);
}

void unlock_mutex(struct Mutex *mutex)
{
        BOOL ret = ReleaseMutex(mutex->handle);
        if (ret == 0)
                fatal("Error detected in ReleaseMutex()");
}

int try_lock_mutex(struct Mutex *mutex)
{
        UNUSED(mutex);
        NOT_IMPLEMENTED();
}