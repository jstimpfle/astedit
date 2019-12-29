#include <astedit/astedit.h>
#include <astedit/textedit.h>
#include <astedit/memoryalloc.h>
#include <astedit/edithistory.h>

enum {
        EDIT_START, /* only the very first item in our history has this kind */
        EDIT_INSERT,
        EDIT_DELETE,
};

struct EditItem {
        struct EditItem *prev;
        struct EditItem *next;
        /* insert or delete. Probably there will never be more. */
        int editKind;
        /* The difference in cursorPosition before and after an editing
         * operation is independent of the actual operation. For example,
         * loading contents from a file into the text editor won't move the
         * cursor position, but keying in some characters will. So for now,
         * we simply store the previous cursor position */
        FILEPOS previousCursorPosition;
        FILEPOS nextCursorPosition;
        /* We need to remember what text was inserted or deleted.
         *  - For insertion edits that we can redo
         *  - For deletion items that we can undo
         */
        FILEPOS editPosition;
        FILEPOS editLength;
        char *editText;
};

static struct EditItem startItem = { .editKind = EDIT_START };
static struct EditItem *editHistory = &startItem;

static void delete_EditItem(struct EditItem *item)
{
        if (item->editText != NULL) {
                FREE_MEMORY(&item->editText);
                ENSURE(item->editText == NULL);
        }
        FREE_MEMORY(&item);
}

/* In the future we will want to have an "undo tree". But for now, when
 * we undo a few items, and then do another operation, we will delete the
 * undone items because they are not reachable anymore (until we improve the
 * data structures) */
static void delete_unreachable_items(void)
{
        while (editHistory->next) {
                struct EditItem *itemToDelete = editHistory->next;
                editHistory->next = editHistory->next->next;
                delete_EditItem(itemToDelete);
        }
}

static void add_item(struct EditItem *item)
{
        delete_unreachable_items();
        item->prev = editHistory;
        item->next = NULL;
        if (item->prev)
                item->prev->next = item;
        editHistory = item;
}

/* XXX: For symmetry, this should probably also erase the copied text from the
 * text rope. But currently the caller code in textedit.c is still doing this.
 */
static void set_item_text(struct EditItem *item, struct TextEdit *edit)
{
        FILEPOS editPosition = item->editPosition;
        FILEPOS editLength = item->editLength;
        ENSURE(item->editText == NULL);
        ALLOC_MEMORY(&item->editText, /*XXX*/(int)editLength + 1);
        char *editText = item->editText;
        copy_text_from_textrope(edit->rope, editPosition, editText, editLength);
        item->editText[editLength] = 0;
}

static void unload_item_text(struct EditItem *item, struct TextEdit *edit)
{
        FILEPOS editPosition = item->editPosition;
        FILEPOS editLength = item->editLength;
        char *editText = item->editText;
        insert_text_into_textrope(edit->rope, editPosition, editText, editLength);
        FREE_MEMORY(&item->editText);
        ENSURE(item->editText == NULL);
}

void record_insert_operation(struct TextEdit *edit, FILEPOS insertionPoint, FILEPOS length,
                             FILEPOS previousCursorPosition, FILEPOS nextCursorPosition)
{
        UNUSED(edit);
        struct EditItem *item;
        ALLOC_MEMORY(&item, 1);
        item->editKind = EDIT_INSERT;
        item->previousCursorPosition = previousCursorPosition;
        item->nextCursorPosition = nextCursorPosition;
        item->editPosition = insertionPoint;
        item->editLength = length;
        item->editText = NULL;
        add_item(item);
}

void record_delete_operation(struct TextEdit *edit, FILEPOS deletionPoint, FILEPOS length,
                             FILEPOS previousCursorPosition, FILEPOS nextCursorPosition)
{
        struct EditItem *item;
        ALLOC_MEMORY(&item, 1);
        item->editKind = EDIT_DELETE;
        item->previousCursorPosition = previousCursorPosition;
        item->nextCursorPosition = nextCursorPosition;
        item->editPosition = deletionPoint;
        item->editLength = length;
        item->editText = NULL;
        set_item_text(item, edit);
        add_item(item);
}

int undo_last_edit_operation(struct TextEdit *edit)
{
        if (editHistory == &startItem)
                return 0;
        struct EditItem *item = editHistory;
        editHistory = item->prev;
        if (item->editKind == EDIT_INSERT) {
                set_item_text(item, edit);
                /* See NOTE at set_item_text() */
                FILEPOS insertionPoint = item->editPosition;
                FILEPOS length = item->editLength;
                erase_text_from_textrope(edit->rope, insertionPoint, length);
        }
        else if (item->editKind == EDIT_DELETE) {
                unload_item_text(item, edit);
        }
        else {
                UNREACHABLE();
        }
        edit->cursorBytePosition = item->previousCursorPosition;
        return 1;
}

int redo_next_edit_operation(struct TextEdit *edit)
{
        if (editHistory->next == NULL)
                return 0;
        editHistory = editHistory->next;
        struct EditItem *item = editHistory;
        if (item->editKind == EDIT_INSERT) {
                unload_item_text(item, edit);
        }
        else if (item->editKind == EDIT_DELETE) {
                set_item_text(item, edit);
                /* See NOTE at set_item_text() */
                FILEPOS editPosition = item->editPosition;
                FILEPOS editLength = item->editLength;
                erase_text_from_textrope(edit->rope, editPosition, editLength);
        }
        else {
                UNREACHABLE();
        }
        edit->cursorBytePosition = item->nextCursorPosition;
        return 1;
}
