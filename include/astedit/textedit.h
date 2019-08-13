#ifndef ASTEDIT_TEXTEDIT_H_INCLUDED
#define ASTEDIT_TEXTEDIT_H_INCLUDED

#include <astedit/astedit.h>
#include <astedit/window.h>
#include <astedit/textrope.h>
#include <astedit/clock.h>  // timer
#include <astedit/filereadthread.h>

struct TextEdit {
        struct Textrope *rope;

        int cursorBytePosition;

        int isSelectionMode;
        int selectionStartBytePosition;

        int firstLineDisplayed;  // need to change this when window size changes, such that cursor is always displayed.
        int numberOfLinesDisplayed;  // should probably be set from outside (reacting to window events)

        /*XXX this stuff probably must be protected with a mutex */
        int isLoading;
        int loadingCompletedBytes;
        int loadingTotalBytes;
        Timer *loadingTimer;
        struct FilereadThreadCtx *loadingFilereadThreadCtx;
};


void init_TextEdit(struct TextEdit *edit);
void exit_TextEdit(struct TextEdit *edit);

void get_selected_range_in_bytes(struct TextEdit *edit, int *outStart, int *outOnePastEnd);
void get_selected_range_in_codepoints(struct TextEdit *edit, int *outStart, int *outOnePastEnd);

void move_view_minimally_to_display_line(struct TextEdit *edit, int lineNumber);
void move_view_minimally_to_display_cursor(struct TextEdit *edit);
void move_cursor_to_byte_position(struct TextEdit *edit, int pos, int isSelecting);
void move_cursor_to_codepoint(struct TextEdit *edit, int codepointPos, int isSelecting);
void move_cursor_left(struct TextEdit *edit, int isSelecting);
void move_cursor_right(struct TextEdit *edit, int isSelecting);
void move_cursor_to_line_and_column(struct TextEdit *edit, int lineNumber, int codepointColumn, int isSelecting);
void move_cursor_lines_relative(struct TextEdit *edit, int linesDiff, int isSelecting);
void move_cursor_up(struct TextEdit *edit, int isSelecting);
void move_cursor_down(struct TextEdit *edit, int isSelecting);
void move_cursor_to_beginning_of_line(struct TextEdit *edit, int isSelecting);
void move_cursor_to_end_of_line(struct TextEdit *edit, int isSelecting);
void move_to_first_line(struct TextEdit *edit, int isSelecting);
void move_to_last_line(struct TextEdit *edit, int isSelecting);
void scroll_up_one_page(struct TextEdit *edit, int isSelecting);
void scroll_down_one_page(struct TextEdit *edit, int isSelecting);




void erase_selected_in_TextEdit(struct TextEdit *edit);
void erase_forwards_in_TextEdit(struct TextEdit *edit);
void erase_backwards_in_TextEdit(struct TextEdit *edit);


void erase_from_textedit(struct TextEdit *edit, int offset, int length);
void insert_codepoint_into_textedit(struct TextEdit *edit, uint32_t codepoint);

/* fill TextEdit with some text loaded from a file. For debugging purposes. */
void textedit_test_init(struct TextEdit *edit, const char *filepath);

#endif
