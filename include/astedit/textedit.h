#ifndef ASTEDIT_TEXTEDIT_H_INCLUDED
#define ASTEDIT_TEXTEDIT_H_INCLUDED

#include <astedit/astedit.h>
#include <astedit/window.h>
#include <astedit/textrope.h>

struct TextEdit {
        struct Textrope *rope;
        int cursorBytePosition;
        int firstLineDisplayed;  // need to change this when window size changes, such that cursor is always displayed.
        int numberOfLinesDisplayed;  // should probably be set from outside (reacting to window events)

        int isSelectionMode;
        int selectionStartBytePosition;
};


void init_TextEdit(struct TextEdit *edit);
void exit_TextEdit(struct TextEdit *edit);

void process_input_in_textEdit(struct Input *input, struct TextEdit *edit);

// length of text contents, in bytes
int textedit_length_in_bytes(struct TextEdit *edit);

/* this costs Log(TextLength) + NumBytes. So try to read larger chunks. Maybe an iteration API would be a good idea */
int read_from_textedit(struct TextEdit *edit, int offset, char *dstBuffer, int size);


void erase_from_textedit(struct TextEdit *edit, int offset, int length);
void insert_codepoint_into_textedit(struct TextEdit *edit, uint32_t codepoint);

/* fill TextEdit with some text loaded from a file. For debugging purposes. */
void textedit_test_init(struct TextEdit *edit, const char *filepath);

#endif
