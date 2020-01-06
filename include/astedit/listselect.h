#ifndef ASTEDIT_GUILISTSELECT_H_INCLUDED
#define ASTEDIT_GUILISTSELECT_H_INCLUDED

#include <astedit/lineedit.h>
#include <astedit/regex.h>

struct ListSelectElem {
        void *data;  // pointer to client data

        // Caption that is used for both displaying and filtering.
        // Client must guarantee that it stays valid while the ListSelect is
        // used.
        const char *caption;
        int captionLength;
};

struct ListSelect {
        // for layout. Needed?
        int x;
        int y;
        int w;
        int h;

        int isConfirmed;
        int isCancelled;

        int isFilterActive;
        int isFilterRegexValid;
        struct LineEdit filterLineEdit;
        struct Regex filterRegex;

        struct ListSelectElem *elems;
        int numElems;
        int selectedElemIndex;  // -1 if none, or 0..numElems-1
};

void setup_ListSelect(struct ListSelect *list);
void teardown_ListSelect(struct ListSelect *list);

void ListSelect_clear_list(struct ListSelect *list);
void ListSelect_append_elem(struct ListSelect *list, const char *caption, int captionLength, void *data);
void ListSelect_select_first_matching_if_filter_does_not_match(struct ListSelect *list);



enum {  // TODO: better name. It's an enumeration of UI actions
        LISTSELECT_ACTION_MOVE_TO_PREV,
        LISTSELECT_ACTION_MOVE_TO_NEXT,
        LISTSELECT_ACTION_CONFIRM_SELECTION,
        LISTSELECT_ACTION_CANCEL_DIALOG,
        NUM_LISTSELECT_ACTION_KINDS
};

void ListSelect_do(struct ListSelect *list, int actionKind /* BUFFERLIST_??? */);

#endif
