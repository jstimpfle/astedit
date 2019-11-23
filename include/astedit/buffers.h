#ifndef ASTEDIT_BUFFERS_H_INCLUDED
#define ASTEDIT_BUFFERS_H_INCLUDED

#include <astedit/astedit.h>
#include <astedit/textedit.h>

struct Buffer {
        struct Buffer *next;
        char *name;
        /* need a better structure later. Is there really a meaningful
         * difference between a textedit and a buffer? Do we want
         * "views"? */
        struct TextEdit textEdit;
};

DATA struct Buffer *buffers;
DATA struct Buffer *currentBuffer;

void switch_to_buffer(struct Buffer *buffer);
struct Buffer *create_new_buffer(const char *name);
void destroy_buffer(struct Buffer *buffer);

#endif
