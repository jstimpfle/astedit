#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
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
        ALLOC_MEMORY(&buf->name, nameLength);
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

void bufferlist_move_to_prev(void)
{
        if (globalData.selectedBuffer && globalData.selectedBuffer->prev != NULL)
                globalData.selectedBuffer = globalData.selectedBuffer->prev;
}

void bufferlist_move_to_next(void)
{
        if (globalData.selectedBuffer && globalData.selectedBuffer->next != NULL)
                globalData.selectedBuffer = globalData.selectedBuffer->next;
}

void bufferlist_confirm_selection(void)
{
        switch_to_buffer(globalData.selectedBuffer);
        globalData.isSelectingBuffer = 0;
}

void bufferlist_cancel_dialog(void)
{
        globalData.isSelectingBuffer = 0;
}

// TODO: need list argument soon
static void (*actionToFunc[NUM_BUFFERLIST_KINDS])(void) = {
#define MAKE(x, y) [x] = y
        MAKE( BUFFERLIST_MOVE_TO_PREV, &bufferlist_move_to_prev ),
        MAKE( BUFFERLIST_MOVE_TO_NEXT, &bufferlist_move_to_next ),
        MAKE( BUFFERLIST_CONFIRM_SELECTION, &bufferlist_confirm_selection ),
        MAKE( BUFFERLIST_CANCEL_DIALOG, &bufferlist_cancel_dialog ),
#undef MAKE
};

void bufferlist_do(int actionKind /* BUFFERLIST_??? */)
{
        ENSURE(0 <= actionKind && actionKind < NUM_BUFFERLIST_KINDS);
        actionToFunc[actionKind]();
}
