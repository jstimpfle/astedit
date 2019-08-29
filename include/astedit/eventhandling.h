#ifndef ASTEDIT_EVENTHANDLING_H_INCLUDED
#define ASTEDIT_EVENTHANDLING_H_INCLUDED

#include <astedit/window.h>
#include <astedit/textedit.h>

/*
void process_input_in_TextEdit(struct Input *input, struct TextEdit *edit);

void process_input_in_TextEdit_with_ViMode(
        struct Input *input, struct TextEdit *edit, struct ViState *state);
        */

void handle_input(struct Input *input, struct TextEdit *edit);

#endif