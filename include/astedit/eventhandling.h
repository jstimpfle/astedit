#include <astedit/window.h>
#include <astedit/textedit.h>

void process_input_in_TextEdit(struct Input *input, struct TextEdit *edit);


enum ViMode {
        VIMODE_NORMAL,
        VIMODE_SELECTING,
        VIMODE_INPUT,
};

struct ViState {
        enum ViMode vimodeKind;
};

void process_input_in_TextEdit_with_ViMode(
        struct Input *input, struct TextEdit *edit, struct ViState *state);
