#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memory.h>
#include <astedit/window.h>
#include <astedit/textedit.h>
#include <astedit/buffers.h>
#include <string.h> // strlen

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
        ALLOC_MEMORY(&buf->name, nameLength + 1);
        copy_string_and_zeroterminate(buf->name, name, nameLength);
        init_TextEdit(&buf->textEdit);

        if (buffers == NULL)
                buffers = buf;
        if (lastBuffer != NULL)
                lastBuffer->next = buf;
        buf->next = NULL;
        buf->prev = lastBuffer;
        lastBuffer = buf;

        return buf;
}

#include <astedit/editor.h>  // globalData
void destroy_buffer(struct Buffer *buffer)
{
        if (buffer->prev == NULL)
                buffers = buffer->next;
        if (buffer->next == NULL)
                lastBuffer = buffer->prev;
        if (buffer->prev != NULL)
                buffer->prev->next = buffer->next;
        if (buffer->next != NULL)
                buffer->next->prev = NULL;

        exit_TextEdit(&buffer->textEdit);
        FREE_MEMORY(&buffer->name);
        FREE_MEMORY(&buffer);
}
