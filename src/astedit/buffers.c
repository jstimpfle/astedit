#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/window.h>
#include <astedit/textedit.h>
#include <string.h> // strlen

struct Buffer {
        struct Buffer *next;
        char *name;
        /* need a better structure later. Is there really a meaningful
         * difference between a textedit and a buffer? Do we want
         * "views"? */
        struct TextEdit textEdit;
};

static struct Buffer *buffers;
static struct Buffer *currentBuffer;

void switch_to_buffer(struct Buffer *buffer)
{
        currentBuffer = buffer;
        activeTextEdit = &buffer->textEdit;
        set_window_title(buffer->name);
}

struct Buffer *create_new_buffer(const char *name)
{
        int nameLength = (int) strlen(name);
        struct Buffer *buf;
        ALLOC_MEMORY(&buf, 1);
        ALLOC_MEMORY(&buf->name, nameLength);
        copy_string_and_zeroterminate(buf->name, name, nameLength);
        init_TextEdit(&buf->textEdit);

        buf->next = buffers;
        buffers = buf;
        return buf;
}

void destroy_buffer(struct Buffer *buffer)
{
        exit_TextEdit(&buffer->textEdit);
        FREE_MEMORY(&buffer->name);
        FREE_MEMORY(&buffer);
}
