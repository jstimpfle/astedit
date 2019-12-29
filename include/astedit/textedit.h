#ifndef ASTEDIT_TEXTEDIT_H_INCLUDED
#define ASTEDIT_TEXTEDIT_H_INCLUDED

#include <astedit/astedit.h>
#include <astedit/window.h>
#include <astedit/textrope.h>
#include <astedit/clock.h>  // timer
#include <astedit/osthread.h>
#include <astedit/texteditloadsave.h>
#include <astedit/filereadwritethread.h>
#include <astedit/vimode.h>

enum {
        NOTIFICATION_INFO,
        NOTIFICATION_ERROR,
};

struct LinescrollAnimation {
        int isActive;
        FILEPOS startLine;
        FILEPOS targetLine;
        float progress;
        struct Timer timer;
};

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

struct TextEdit {
        struct Textrope *rope;

        /* Filepath that is used for saving. Can be NULL */
        char *filepath;

        FILEPOS cursorBytePosition;
        FILEPOS firstLineDisplayed;  // need to change this when window size changes, such that cursor is always displayed.
        FILEPOS numberOfLinesDisplayed;  // should probably be set from outside (reacting to window events)

        struct EditItem startItem; // sentinel
        struct EditItem *editHistory;

        int isVimodeActive;
        struct ViState vistate;

        int isSelectionMode;
        FILEPOS selectionStartBytePosition;

        struct LinescrollAnimation scrollAnimation;
        struct TextEditLoadingCtx loading;
        struct TextEditSavingCtx saving;

        int haveNotification;
        int notificationLength;
        int notificationKind;
        char notificationBuffer[1024];
};


void init_TextEdit(struct TextEdit *edit);
void exit_TextEdit(struct TextEdit *edit);


/* insert codepoints. They will be stored UTF-8 encoded in the rope. This
 * function might be removed later since it is not very inefficient. */
void insert_codepoints_into_textedit(struct TextEdit *edit, FILEPOS insertPos, uint32_t *codepoints, int numCodepoints);

/* Just insert plain bytes. The rope shouldn't care much whether it is valid
 * UTF-8 encoded text. Some other parts will still crash on invalid UTF-8 but
 * we will work on that. */
void insert_text_into_textedit(struct TextEdit *edit, FILEPOS insertPos, const char *text, FILEPOS length,
                                      FILEPOS nextCursorPosition);

void get_selected_range_in_bytes(struct TextEdit *edit, FILEPOS *outStart, FILEPOS *outOnePastEnd);
void get_selected_range_in_codepoints(struct TextEdit *edit, FILEPOS *outStart, FILEPOS *outOnePastEnd);


void move_cursor_lines_relative(struct TextEdit *edit, FILEPOS linesDiff, int isSelecting);

void delete_current_line(struct TextEdit *edit);  // could this be done with a MOVEMENT?
void erase_selected_in_TextEdit(struct TextEdit *edit);

void send_notification_to_textedit(struct TextEdit *edit, int notificationKind, const char *message, int messageLength);
void send_notification_to_textedit_f(struct TextEdit *edit, int notificationKind, const char *fmt, ...);

void insert_codepoint_into_textedit(struct TextEdit *edit, uint32_t codepoint);

/* TODO: maybe introduce TIMETICK event or sth like that, to
handle updates as part of event processing? */
void update_textedit(struct TextEdit *edit);

/* XXX: currently we have one active text edit that receive the input keys */
DATA struct TextEdit *activeTextEdit;


#endif
