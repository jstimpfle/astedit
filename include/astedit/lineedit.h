#ifndef ASTEDIT_LINEEDIT_H_INCLUDED
#define ASTEDIT_LINEEDIT_H_INCLUDED

/* "widget" suitable for small single-line inputs. */

#include <astedit/astedit.h>

struct LineEdit {
        char buf[1024]; // TODO
        int fill;
        int cursorBytePosition;
        int isAborted;
        int isConfirmed;
};

void LineEdit_set_contents_from_string(struct LineEdit *lineEdit, const char *string, int length);
void LineEdit_insert_codepoint(uint32_t codepoint, struct LineEdit *lineEdit);
void LineEdit_erase_backwards(struct LineEdit *lineEdit);
void LineEdit_erase_forwards(struct LineEdit *lineEdit);

void LineEdit_move_cursor_to_beginning(struct LineEdit *lineEdit);
void LineEdit_move_cursor_to_end(struct LineEdit *lineEdit);
void LineEdit_move_cursor_left(struct LineEdit *lineEdit);
void LineEdit_move_cursor_right(struct LineEdit *lineEdit);

#endif
