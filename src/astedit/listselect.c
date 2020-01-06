#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/listselect.h>

void setup_ListSelect(struct ListSelect *list)
{
        ZERO_MEMORY(list);
}

void teardown_ListSelect(struct ListSelect *list)
{
        ZERO_ARRAY(list->elems, list->numElems);
        FREE_MEMORY(&list->elems);
        ZERO_MEMORY(list);
}

void ListSelect_clear_list(struct ListSelect *list)
{
        ZERO_ARRAY(list->elems, list->numElems);
        FREE_MEMORY(&list->elems);
        list->elems = NULL;
        list->numElems = 0;
}

void ListSelect_append_elem(struct ListSelect *list, const char *caption, int captionLength, void *data)
{
        int index = list->numElems ++;
        REALLOC_MEMORY(&list->elems, list->numElems);
        list->elems[index].data = data;
        list->elems[index].caption = caption;
        list->elems[index].captionLength = captionLength;
}




static int is_valid_selection(struct ListSelect *list, int index)
{
        ENSURE(0 <= index && index < list->numElems);
        struct ListSelectElem *elem = &list->elems[index];
        if (!list->isFilterActive)
                return 1;
        if (!list->isFilterRegexValid)
                return 0; //XXX ???
        return match_regex(&list->filterRegex,
                           elem->caption, elem->captionLength);
}

static void ListSelect_move_to_prev(struct ListSelect *list)
{
        if (list->selectedElemIndex > 0) {
                for (int i = list->selectedElemIndex;
                     i --> 0;) {
                        if (is_valid_selection(list, i)) {
                                list->selectedElemIndex = i;
                                return;
                        }
                }
        }
}

static void ListSelect_move_to_next(struct ListSelect *list)
{
        if (list->selectedElemIndex + 1 < list->numElems) {
                for (int i = list->selectedElemIndex + 1;
                     i < list->numElems; i++) {
                        if (is_valid_selection(list, i)) {
                                list->selectedElemIndex = i;
                                return;
                        }
                }
        }
}

static void ListSelect_confirm_selection(struct ListSelect *list)
{
        list->isConfirmed = 1;
}

static void ListSelect_cancel_dialog(struct ListSelect *list)
{
        list->isCancelled = 1;
}

// TODO: need list argument soon
static void (*actionToFunc[NUM_LISTSELECT_ACTION_KINDS])(struct ListSelect *list) = {
#define MAKE(x, y) [x] = y
        MAKE( LISTSELECT_ACTION_MOVE_TO_PREV, &ListSelect_move_to_prev ),
        MAKE( LISTSELECT_ACTION_MOVE_TO_NEXT, &ListSelect_move_to_next ),
        MAKE( LISTSELECT_ACTION_CONFIRM_SELECTION, &ListSelect_confirm_selection ),
        MAKE( LISTSELECT_ACTION_CANCEL_DIALOG, &ListSelect_cancel_dialog ),
#undef MAKE
};

void ListSelect_do(struct ListSelect *list, int actionKind)
{
        ENSURE(0 <= actionKind && actionKind < NUM_LISTSELECT_ACTION_KINDS);
        actionToFunc[actionKind](list);
}
