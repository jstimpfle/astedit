#include <astedit/astedit.h>
#include <astedit/memory.h>
#include <astedit/logging.h>
#include <astedit/mutex.h>
#include <errno.h>
#include <string.h> // strerror()
#include <pthread.h>

struct Mutex {
        pthread_mutex_t handle;
};

static void NORETURN fatal_pthreads(int r, const char *function)
{
        fatalf("Error from pthreads library after calling %s(): %s\n",
               strerror(r));
}

static inline void check_pthreads_error(int r, const char *function)
{
        if (r != 0)
                fatal_pthreads(r, function);
}

struct Mutex *create_mutex(void)
{
        pthread_mutex_t handle;
        int r = pthread_mutex_init(&handle, NULL);
        check_pthreads_error(r, "pthread_mutex_init");
        struct Mutex *mutex;
        ALLOC_MEMORY(&mutex, 1);
        mutex->handle = handle;
        return mutex;
}

void destroy_mutex(struct Mutex *mutex)
{
        int r = pthread_mutex_destroy(&mutex->handle);
        check_pthreads_error(r, "pthread_mutex_destroy");
        FREE_MEMORY(&mutex);
}

void lock_mutex(struct Mutex *mutex)
{
        int r = pthread_mutex_lock(&mutex->handle);
        check_pthreads_error(r, "pthread_mutex_lock");
}

void unlock_mutex(struct Mutex *mutex)
{
        int r = pthread_mutex_unlock(&mutex->handle);
        check_pthreads_error(r, "pthread_mutex_unlock");
}

int try_lock_mutex(struct Mutex *mutex)
{
        int r = pthread_mutex_trylock(&mutex->handle);
        if (r == 0)
                return 1;
        if (r == EBUSY)
                return 0;
        fatal_pthreads(r, "pthread_mutex_trylock");
}
