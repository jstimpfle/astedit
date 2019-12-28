#ifndef ASTEDIT_BUFFERS_H_INCLUDED
#define ASTEDIT_BUFFERS_H_INCLUDED

#include <astedit/astedit.h>
#include <astedit/textedit.h>

enum {  // TODO: better name. It's an enumeration of UI actions
        BUFFERLIST_MOVE_TO_PREV,
        BUFFERLIST_MOVE_TO_NEXT,
        BUFFERLIST_CONFIRM_SELECTION,
        BUFFERLIST_CANCEL_DIALOG,
        NUM_BUFFERLIST_KINDS
};

struct Buffer {
        struct Buffer *prev;
        struct Buffer *next;
        char *name;
        /* need a better structure later. Is there really a meaningful
         * difference between a textedit and a buffer? Do we want
         * "views"? */
        struct TextEdit textEdit;
};

DATA struct Buffer *buffers;
DATA struct Buffer *currentBuffer;
DATA struct Buffer *lastBuffer;

void switch_to_buffer(struct Buffer *buffer);
struct Buffer *create_new_buffer(const char *name);
void destroy_buffer(struct Buffer *buffer);

void bufferlist_do(int actionKind /* BUFFERLIST_??? */);

#endif
