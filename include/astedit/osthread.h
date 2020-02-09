#ifndef ASTEDIT_OSTHREAD_H_INCLUDED
#define ASTEDIT_OSTHREAD_H_INCLUDED

struct OsThreadHandle;

typedef void OsThreadEntryFunc(void *param);

struct OsThreadHandle *create_and_start_thread(OsThreadEntryFunc *entryFunc, void *param);
int check_if_thread_has_exited(struct OsThreadHandle *handle);
void wait_for_thread_to_end(struct OsThreadHandle *handle);
// warning: don't call this, normally. Resource leakage!
void cancel_thread_and_wait(struct OsThreadHandle *handle);  // TODO: is this sync or async?
void dispose_thread(struct OsThreadHandle *handle);  // requires terminated thread

#endif
