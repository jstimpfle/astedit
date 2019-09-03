#ifndef ASTEDIT_TEXTEDIT_H_INCLUDED
#define ASTEDIT_TEXTEDIT_H_INCLUDED

#include <astedit/astedit.h>
#include <astedit/window.h>
#include <astedit/textrope.h>
#include <astedit/clock.h>  // timer
#include <astedit/osthread.h>
#include <astedit/filereadwritethread.h>
#include <astedit/vimode.h>


struct TextEdit {
        struct Textrope *rope;

        FILEPOS cursorBytePosition;
        FILEPOS firstLineDisplayed;  // need to change this when window size changes, such that cursor is always displayed.
        FILEPOS numberOfLinesDisplayed;  // should probably be set from outside (reacting to window events)

        int isVimodeActive;
        struct ViState vistate;

        int isSelectionMode;
        FILEPOS selectionStartBytePosition;

        /***ANIMATION***/
        int isAnimationActive;
        FILEPOS animationStartLine;
        FILEPOS animationTargetLine;
        float animationProgress;
        Timer *animationTimer;

        /***LOADING***/

        int isLoading;
        struct FilereadThreadCtx *loadingThreadCtx;
        struct OsThreadHandle *loadingThreadHandle;

        /*XXX this stuff is set by a separate read thread,
        so probably must be protected with a mutex */
        /*(XXX: as well as the Textrope!!!)*/
        int isLoadingCompleted;  /*the below numbers might be wrong
                                 (files can grow while they are being read),
                                 and that's why we have this explicit
                                 "completed" flag */
        FILEPOS loadingCompletedBytes;  // NOTE: completed bytes of input file
        FILEPOS loadingTotalBytes;
        Timer *loadingTimer;

        char loadingBuffer[512];  // TODO: heap alloc?
        int loadingBufferFill;  // fill from start


        /***SAVING***/
        /* only one of loading or saving is active at any one time.
        We could use a union to make it clearer */
        int isSaving;
        struct FilewriteThreadCtx *savingThreadCtx;
        struct OsThreadHandle *savingThreadHandle;

        /* this stuff is set by a separate writer thread. */
        int isSavingCompleted;
        FILEPOS savingCompletedBytes;  // NOTE: completed bytes of internal storage
        FILEPOS savingTotalBytes;
        Timer *savingTimer;

        char savingBuffer[512];
        int savingBufferFill;
};


void init_TextEdit(struct TextEdit *edit);
void exit_TextEdit(struct TextEdit *edit);

void insert_codepoints_into_textedit(struct TextEdit *edit, FILEPOS insertPos, uint32_t *codepoints, int numCodepoints);

void get_selected_range_in_bytes(struct TextEdit *edit, FILEPOS *outStart, FILEPOS *outOnePastEnd);
void get_selected_range_in_codepoints(struct TextEdit *edit, FILEPOS *outStart, FILEPOS *outOnePastEnd);

void move_view_minimally_to_display_line(struct TextEdit *edit, FILEPOS lineNumber);
void move_view_minimally_to_display_cursor(struct TextEdit *edit);

void move_cursor_to_byte_position(struct TextEdit *edit, FILEPOS pos, int isSelecting);
void move_cursor_to_codepoint(struct TextEdit *edit, FILEPOS codepointPos, int isSelecting);


enum MovementKind {
        MOVEMENT_LEFT,
        MOVEMENT_RIGHT,
        MOVEMENT_UP,
        MOVEMENT_DOWN,
        MOVEMENT_NEXT_CODEPOINT,  // like RIGHT but won't stop at end of line
        MOVEMENT_PREVIOUS_CODEPOINT,  // like LEFT but won't stop at beginning of line
        MOVEMENT_PAGEUP,
        MOVEMENT_PAGEDOWN,
        MOVEMENT_LINEBEGIN,
        MOVEMENT_LINEEND,
        MOVEMENT_FIRSTLINE,
        MOVEMENT_LASTLINE,
        MOVEMENT_SPECIFICLINE,
        MOVEMENT_SPECIFICLINEANDCOLUMN,
};

struct Movement {
        enum MovementKind movementKind;
        FILEPOS pos1;  // when necessary, e.g. SPECIFICLINE
        FILEPOS pos2;  // when necessary, e.g. SPECIFICLINEANDCOLUMN
};

void move_cursor_with_movement(struct TextEdit *edit, struct Movement *movement, int isSelecting);
void delete_with_movement(struct TextEdit *edit, struct Movement *movement);

#define MOVEMENT(...) (&(struct Movement){__VA_ARGS__})
#define MOVE(...) move_cursor_with_movement(edit, MOVEMENT(__VA_ARGS__), isSelecting)
#define DELETE(...) delete_with_movement(edit, MOVEMENT(__VA_ARGS__))

static inline void move_cursor_left(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_LEFT); }
static inline void move_cursor_right(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_RIGHT); }
static inline void move_cursor_up(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_UP); }
static inline void move_cursor_down(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_DOWN); }
static inline void move_cursor_to_next_codepoint(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_NEXT_CODEPOINT); }
static inline void move_cursor_to_previous_codepoint(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_PREVIOUS_CODEPOINT); }
static inline void move_cursor_to_beginning_of_line(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_LINEBEGIN); }
static inline void move_cursor_to_end_of_line(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_LINEEND); }
static inline void move_cursor_to_line_and_column(struct TextEdit *edit, FILEPOS lineNumber, FILEPOS columnCodepoint, int isSelecting) { MOVE(MOVEMENT_SPECIFICLINEANDCOLUMN, lineNumber, columnCodepoint); }
static inline void move_cursor_to_line(struct TextEdit *edit, FILEPOS lineNumber, int isSelecting) { MOVE(MOVEMENT_SPECIFICLINE, lineNumber); }
static inline void move_cursor_to_first_line(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_FIRSTLINE); }
static inline void move_cursor_to_last_line(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_LASTLINE); }

static inline void delete_left(struct TextEdit *edit) { DELETE(MOVEMENT_LEFT); }
static inline void delete_right(struct TextEdit *edit) { DELETE(MOVEMENT_RIGHT); }
static inline void delete_up(struct TextEdit *edit) { DELETE(MOVEMENT_UP); }
static inline void delete_down(struct TextEdit *edit) { DELETE(MOVEMENT_DOWN); }
static inline void delete_to_next_codepoint(struct TextEdit *edit) { DELETE(MOVEMENT_NEXT_CODEPOINT); }
static inline void delete_to_previous_codepoint(struct TextEdit *edit) { DELETE(MOVEMENT_PREVIOUS_CODEPOINT); }
static inline void delete_to_beginning_of_line(struct TextEdit *edit) { DELETE(MOVEMENT_LINEBEGIN); }
static inline void delete_to_end_of_line(struct TextEdit *edit) { DELETE(MOVEMENT_LINEEND); }
static inline void delete_to_line_and_column(struct TextEdit *edit, FILEPOS lineNumber, FILEPOS columnCodepoint) { DELETE(MOVEMENT_SPECIFICLINEANDCOLUMN, lineNumber, columnCodepoint); }
static inline void delete_to_line(struct TextEdit *edit, FILEPOS lineNumber) { DELETE(MOVEMENT_SPECIFICLINE, lineNumber); }
static inline void delete_to_first_line(struct TextEdit *edit) { DELETE(MOVEMENT_FIRSTLINE); }
static inline void delete_to_last_line(struct TextEdit *edit) { DELETE(MOVEMENT_LASTLINE); }

void move_cursor_lines_relative(struct TextEdit *edit, FILEPOS linesDiff, int isSelecting);

void delete_current_line(struct TextEdit *edit);  // could this be done with a MOVEMENT?


void scroll_up_one_page(struct TextEdit *edit, int isSelecting);
void scroll_down_one_page(struct TextEdit *edit, int isSelecting);




void erase_selected_in_TextEdit(struct TextEdit *edit);
void erase_forwards_in_TextEdit(struct TextEdit *edit);
void erase_backwards_in_TextEdit(struct TextEdit *edit);


void insert_codepoint_into_textedit(struct TextEdit *edit, uint32_t codepoint);

/* TODO: maybe introduce TIMETICK event or sth like that, to
handle updates as part of event processing? */
void update_textedit(struct TextEdit *edit);

/* fill TextEdit with some text loaded from a file. For debugging purposes. */
void textedit_test_init(struct TextEdit *edit, const char *filepath);



#endif
