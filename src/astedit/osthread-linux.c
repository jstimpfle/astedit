#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <astedit/memoryalloc.h>
#include <astedit/osthread.h>
#include <errno.h>
#include <string.h> // strerror
#include <pthread.h>

struct OsThreadHandle {
        pthread_t threadHandle;
        OsThreadEntryFunc *entryFunc;
        void *param;
        // I don't know a way to use pthreads to check if the thread is
        // finished, so here is this variable.
        int isFinished;
        //
        int waitedForThreadEnd;
};

static void *thread_adapter_linux(void *param)
{
        struct OsThreadHandle *handle = param;
        (*handle->entryFunc)(handle->param);
        handle->isFinished = 1;
        return NULL;
}


struct OsThreadHandle *create_and_start_thread(OsThreadEntryFunc *entryFunc, void *param)
{
        struct OsThreadHandle *handle;
        ALLOC_MEMORY(&handle, 1);

        handle->isFinished = 0;
        handle->waitedForThreadEnd = 0;
        handle->entryFunc = entryFunc;
        handle->param = param;
        int ret = pthread_create(&handle->threadHandle,
                                 NULL, &thread_adapter_linux, handle);
        if (ret != 0)
                fatalf("Failed to create thread: %s\n", strerror(ret));

        return handle;
}

int check_if_thread_has_exited(struct OsThreadHandle *handle)
{
        return handle->isFinished;
}

void wait_for_thread_to_end(struct OsThreadHandle *handle)
{
        if (!handle->waitedForThreadEnd) {
                int ret = pthread_join(handle->threadHandle, NULL);
                ENSURE(ret == 0);
                handle->waitedForThreadEnd = 1;
        }
}

void cancel_thread_and_wait(struct OsThreadHandle *handle)
{
        int ret = pthread_cancel(handle->threadHandle);
        /* it can happen that the thread is already terminated. In this case
           ENOSRCH error (No Such Process) is returned. */
        ENSURE(ret == 0 || ret == ESRCH);
        /* We need to pthread_join(), whether ESRC was returned or not.
         * Otherwise there will be resource leakage. */
        wait_for_thread_to_end(handle);
}

void dispose_thread(struct OsThreadHandle *handle)
{
        ENSURE(check_if_thread_has_exited(handle));
        wait_for_thread_to_end(handle);
        FREE_MEMORY(&handle);
}
