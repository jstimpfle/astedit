#include <astedit/astedit.h>
#include <astedit/textedit.h>
#include <astedit/memoryalloc.h>
#include <astedit/edithistory.h>

enum {
        EDIT_INSERT,
        EDIT_DELETE,
};

struct InsertOperation {
        FILEPOS insertionPoint;
        FILEPOS length;
};

struct DeleteOperation {
        FILEPOS deletionPoint;
        FILEPOS length;
        char *deletedText;
};

struct EditItem {
        struct EditItem *prev;
        /* The difference in cursorPosition before and after an editing
         * operation is independent of the actual operation. For example,
         * loading contents from a file into the text editor won't move the
         * cursor position, but keying in some characters will. So for now,
         * we simply store the previous cursor position */
        FILEPOS previousCursorPosition;
        int editKind;
        union {
                struct InsertOperation tInsert;
                struct DeleteOperation tDelete;
        } data;
};

static struct EditItem *editHistory;

void record_insert_operation(struct TextEdit *edit, FILEPOS insertionPoint, FILEPOS length, FILEPOS previousCursorPosition)
{
        struct EditItem *item;
        ALLOC_MEMORY(&item, 1);
        item->previousCursorPosition = previousCursorPosition;
        item->editKind = EDIT_INSERT;
        item->data.tInsert.insertionPoint = insertionPoint;
        item->data.tInsert.length = length;
        item->prev = editHistory;
        editHistory = item;
}

void record_delete_operation(struct TextEdit *edit, FILEPOS deletionPoint, FILEPOS length, FILEPOS previousCursorPosition)
{
        char *deletedText;
        ALLOC_MEMORY(&deletedText, length + 1);
        copy_text_from_textrope(edit->rope, deletionPoint, deletedText, length);
        deletedText[length] = 0;
        struct EditItem *item;
        ALLOC_MEMORY(&item, 1);
        item->prev = NULL;
        item->previousCursorPosition = previousCursorPosition;
        item->editKind = EDIT_DELETE;
        item->data.tDelete.deletionPoint = deletionPoint;
        item->data.tDelete.length = length;
        item->data.tDelete.deletedText = deletedText;
        item->prev = editHistory;
        editHistory = item;
}

int undo_last_edit_operation(struct TextEdit *edit)
{
        if (editHistory == NULL)
                return 0;
        struct EditItem *item = editHistory;
        editHistory = item->prev;
        edit->cursorBytePosition = item->previousCursorPosition;
        if (item->editKind == EDIT_INSERT) {
                FILEPOS insertionPoint = item->data.tInsert.insertionPoint;
                FILEPOS length = item->data.tInsert.length;
                erase_text_from_textrope(edit->rope, insertionPoint, length);
                /* we might need to record the previous cursorBytePosition as
                 * part of the operation */
        }
        else if (item->editKind == EDIT_DELETE) {
                FILEPOS deletionPoint = item->data.tDelete.deletionPoint;
                FILEPOS length = item->data.tDelete.length;
                char *deletedText = item->data.tDelete.deletedText;
                insert_text_into_textrope(edit->rope, deletionPoint, deletedText, length);
                /*XXX: this deletedText was provided by the caller. We shouldn't
                 * know how to delete it. Instead, the record-deletion interface
                 * should be changed such that we make the copy on our own */
                FREE_MEMORY(&item->data.tDelete.deletedText);
        }
        FREE_MEMORY(&item);
        return 1;
}
