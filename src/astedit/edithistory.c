#include <astedit/astedit.h>
#include <astedit/textedit.h>
#include <astedit/memory.h>
#include <astedit/edithistory.h>


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
static void delete_unreachable_items(struct TextEdit *edit)
{
        while (edit->editHistory->next) {
                struct EditItem *itemToDelete = edit->editHistory->next;
                edit->editHistory->next = edit->editHistory->next->next;
                delete_EditItem(itemToDelete);
        }
}

static void add_item(struct TextEdit *edit, struct EditItem *item)
{
        delete_unreachable_items(edit);
        item->prev = edit->editHistory;
        item->next = NULL;
        if (item->prev)
                item->prev->next = item;
        edit->editHistory = item;
}

/* XXX: For symmetry, this should probably also erase the copied text from the
 * text rope. But currently the caller code in textedit.c is still doing this.
 */
static void set_item_text(struct TextEdit *edit, struct EditItem *item)
{
        FILEPOS editPosition = item->editPosition;
        FILEPOS editLength = item->editLength;
        ENSURE(item->editText == NULL);
        ALLOC_MEMORY(&item->editText, /*XXX*/(int)editLength + 1);
        char *editText = item->editText;
        copy_text_from_textrope(edit->rope, editPosition, editText, editLength);
        item->editText[editLength] = 0;
}

static void unload_item_text(struct TextEdit *edit, struct EditItem *item)
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
        add_item(edit, item);
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
        set_item_text(edit, item);
        add_item(edit, item);
}

int undo_last_edit_operation(struct TextEdit *edit)
{
        if (edit->editHistory == &edit->startItem)
                return 0;
        struct EditItem *item = edit->editHistory;
        edit->editHistory = item->prev;
        if (item->editKind == EDIT_INSERT) {
                set_item_text(edit, item);
                /* See NOTE at set_item_text() */
                FILEPOS insertionPoint = item->editPosition;
                FILEPOS length = item->editLength;
                erase_text_from_textrope(edit->rope, insertionPoint, length);
        }
        else if (item->editKind == EDIT_DELETE) {
                unload_item_text(edit, item);
        }
        else {
                UNREACHABLE();
        }
        edit->cursorBytePosition = item->previousCursorPosition;
        return 1;
}

int redo_next_edit_operation(struct TextEdit *edit)
{
        if (edit->editHistory->next == NULL)
                return 0;
        edit->editHistory = edit->editHistory->next;
        struct EditItem *item = edit->editHistory;
        if (item->editKind == EDIT_INSERT) {
                unload_item_text(edit, item);
        }
        else if (item->editKind == EDIT_DELETE) {
                set_item_text(edit, item);
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
