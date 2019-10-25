#ifndef ASTEDIT_TEXTEDITLOADSAVE_H_INCLUDED
#define ASTEDIT_TEXTEDITLOADSAVE_H_INCLUDED

#include <astedit/astedit.h>
#include <astedit/filereadwritethread.h>
#include <astedit/mutex.h>
#include <astedit/textedit.h>
#include <astedit/textrope.h>

struct TextEditLoadingCtx {
        struct Mutex *mutex;
        struct TextEdit *edit;  // loading target

        int isActive;
        int isCompleted;

        struct FilereadThreadCtx filereadThreadCtx;
        struct OsThreadHandle *threadHandle;

        FILEPOS completedBytes;  // NOTE: completed bytes of input file
        FILEPOS totalBytes;
        struct Timer timer;

        char buffer[512];  // TODO: heap alloc?
        int bufferFill;  // fill from start
};

struct TextEditSavingCtx {
        struct Mutex *mutex;
        struct Textrope *rope;  // holds contents that should be saved

        int isActive;
        int isCompleted;

        struct FilewriteThreadCtx filewriteThreadCtx;
        struct OsThreadHandle *threadHandle;

        /* this stuff is set by a separate writer thread. */
        FILEPOS completedBytes;  // NOTE: completed bytes of internal storage
        FILEPOS totalBytes;
        struct Timer timer;

        char buffer[512];
        int bufferFill;
};

void load_file_to_textedit(struct TextEditLoadingCtx *loading, const char *filepath, int filepathLength, struct TextEdit *edit);
void write_textrope_contents_to_file(struct TextEditSavingCtx *saving, struct Textrope *rope, const char *filepath, int filepathLength);

int check_if_loading_completed_and_if_so_then_cleanup(struct TextEditLoadingCtx *ctx);
int check_if_saving_completed_and_if_so_then_cleanup(struct TextEditSavingCtx *ctx);

#endif
