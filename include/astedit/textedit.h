#ifndef ASTEDIT_TEXTEDIT_H_INCLUDED
#define ASTEDIT_TEXTEDIT_H_INCLUDED

#include <astedit/astedit.h>
#include <astedit/window.h>
#include <astedit/textrope.h>
#include <astedit/clock.h>  // timer
#include <astedit/filereadthread.h>


/* Optional VI mode. */

enum ViMode {
        VIMODE_NORMAL,
        VIMODE_SELECTING,
        VIMODE_INPUT,
        NUM_VIMODE_KINDS,
};

enum ViNormalModeModal {
        VIMODAL_NORMAL,
        VIMODAL_D,
};

struct ViState {
        enum ViMode vimodeKind;
        enum ViNormalModeModal modalKind;
};

extern const char *const vimodeKindString[NUM_VIMODE_KINDS];


struct TextEdit {
        struct Textrope *rope;

        FILEPOS cursorBytePosition;
        FILEPOS firstLineDisplayed;  // need to change this when window size changes, such that cursor is always displayed.
        FILEPOS numberOfLinesDisplayed;  // should probably be set from outside (reacting to window events)

        int isSelectionMode;
        FILEPOS selectionStartBytePosition;


        /*XXX this stuff probably must be protected with a mutex */
        int isLoading;
        FILEPOS loadingCompletedBytes;
        FILEPOS loadingTotalBytes;

        char loadingBuffer[512];  // TODO: heap alloc?
        int loadingBufferFill;  // fill from start

        Timer *loadingTimer;
        struct FilereadThreadHandle *loadingThreadHandle;

        int isVimodeActive;
        struct ViState vistate;
};


void init_TextEdit(struct TextEdit *edit);
void exit_TextEdit(struct TextEdit *edit);

void get_selected_range_in_bytes(struct TextEdit *edit, FILEPOS *outStart, FILEPOS *outOnePastEnd);
void get_selected_range_in_codepoints(struct TextEdit *edit, FILEPOS *outStart, FILEPOS *outOnePastEnd);

void move_view_minimally_to_display_line(struct TextEdit *edit, FILEPOS lineNumber);
void move_view_minimally_to_display_cursor(struct TextEdit *edit);
void move_cursor_to_byte_position(struct TextEdit *edit, FILEPOS pos, int isSelecting);
void move_cursor_to_codepoint(struct TextEdit *edit, FILEPOS codepointPos, int isSelecting);
void move_cursor_left(struct TextEdit *edit, int isSelecting);
void move_cursor_right(struct TextEdit *edit, int isSelecting);
void move_cursor_to_line_and_column(struct TextEdit *edit, FILEPOS lineNumber, FILEPOS codepointColumn, int isSelecting);
void move_cursor_lines_relative(struct TextEdit *edit, FILEPOS linesDiff, int isSelecting);
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
