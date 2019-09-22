#ifndef ASTEDIT_MUTEX_H_INCLUDED
#define ASTEDIT_MUTEX_H_INCLUDED

struct Mutex;

struct Mutex *create_mutex(void);
void destroy_mutex(struct Mutex *mutex);

void lock_mutex(struct Mutex *mutex);
void unlock_mutex(struct Mutex *mutex);

int try_lock_mutex(struct Mutex *mutex);  // Never blocks. Returns 0 iff successful.

#endif
