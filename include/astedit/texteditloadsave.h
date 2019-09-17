#ifndef ASTEDIT_TEXTEDITLOADSAVE_H_INCLUDED
#define ASTEDIT_TEXTEDITLOADSAVE_H_INCLUDED

#include <astedit/astedit.h>
#include <astedit/textedit.h>
#include <astedit/textrope.h>

struct TextEditLoadingCtx {
        struct Textrope *rope;  // loading target

        int isActive;
        struct FilereadThreadCtx *threadCtx;
        struct OsThreadHandle *threadHandle;

        /*XXX this stuff is set by a separate read thread,
        so probably must be protected with a mutex */
        /*(XXX: as well as the Textrope!!!)*/
        int isCompleted;  /*the below numbers might be wrong
                                 (files can grow while they are being read),
                                 and that's why we have this explicit
                                 "completed" flag */
        FILEPOS completedBytes;  // NOTE: completed bytes of input file
        FILEPOS totalBytes;
        Timer *timer;

        char buffer[512];  // TODO: heap alloc?
        int bufferFill;  // fill from start
};

struct TextEditSavingCtx {
        struct Textrope *rope;  // holds contents that should be saved

        int isActive;
        struct FilewriteThreadCtx *threadCtx;
        struct OsThreadHandle *threadHandle;

        /* this stuff is set by a separate writer thread. */
        int isCompleted;
        FILEPOS completedBytes;  // NOTE: completed bytes of internal storage
        FILEPOS totalBytes;
        Timer *timer;

        char buffer[512];
        int bufferFill;
};

void load_file_to_textrope(struct TextEditLoadingCtx *loading, const char *filepath, int filepathLength, struct Textrope *rope);
void write_textrope_contents_to_file(struct TextEditSavingCtx *saving, struct Textrope *rope, const char *filepath, int filepathLength);

#endif
