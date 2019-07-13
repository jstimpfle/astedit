#include <astedit/astedit.h>
#include <astedit/window.h>

/* for now - simplicity! */

struct TextEdit {
        char *contents;
        int length;
};


void init_TextEdit(struct TextEdit *edit);
void exit_TextEdit(struct TextEdit *edit);

void process_input_in_textEdit(struct Input *input, struct TextEdit *edit);
