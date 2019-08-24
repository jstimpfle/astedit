#include <astedit/astedit.h>
#include <astedit/eventhandling.h>

#if 0
void process_input_in_TextEdit_line(struct Input *input, struct TextEdit *edit)
{
        if (input->inputKind == INPUT_KEY) {
                int modifiers = input->data.tKey.modifiers;
                int isSelecting = modifiers & MODIFIER_SHIFT;  // only relevant for some inputs
                switch (input->data.tKey.keyKind) {
                case KEY_ENTER:
                        // what to do?
                        break;
                case KEY_CURSORLEFT:
                        move_cursor_left(edit, isSelecting);
                        break;
                case KEY_CURSORRIGHT:
                        move_cursor_right(edit, isSelecting);
                        break;
                case KEY_HOME:
                        if (modifiers & MODIFIER_CONTROL)
                                move_to_first_line(edit, isSelecting);
                        else
                                move_cursor_to_beginning_of_line(edit, isSelecting);
                        break;
                case KEY_END:
                        if (modifiers & MODIFIER_CONTROL)
                                move_to_last_line(edit, isSelecting);
                        else
                                move_cursor_to_end_of_line(edit, isSelecting);
                        break;
                case KEY_DELETE:
                        if (edit->isSelectionMode)
                                erase_selected_in_TextEdit(edit);
                        else
                                erase_forwards_in_TextEdit(edit);
                        break;
                case KEY_BACKSPACE:
                        if (edit->isSelectionMode)
                                erase_selected_in_TextEdit(edit);
                        else
                                erase_backwards_in_TextEdit(edit);
                        break;
                default:
                        if (input->data.tKey.hasCodepoint) {
                                if (edit->isSelectionMode)
                                        erase_selected_in_TextEdit(edit);
                                unsigned long codepoint = input->data.tKey.codepoint;
                                insert_codepoint_into_textedit(edit, codepoint);
                                //debug_check_textrope(edit->rope);
                        }
                        break;
                }


        }
}
#endif




enum ViMovement {
        VI_MOVEMENT_LEFT,
        VI_MOVEMENT_RIGHT,
        VI_MOVEMENT_UP,
        VI_MOVEMENT_DOWN,
        VI_MOVEMENT_WORD_FORWARDS,
        VI_MOVEMENT_WORD_BACKWARDS,
        VI_MOVEMENT_BEGINNING_OF_LINE,
        VI_MOVEMENT_END_OF_LINE,
        VI_MOVEMENT_FIRST_LINE,
        VI_MOVEMENT_LAST_LINE,
};



static void process_movements_in_ViMode_NORMAL_or_SELECTING(
        struct Input *input, struct TextEdit *edit, struct ViState *state)
{
        UNUSED(state);
        int isSelectionMode = edit->isSelectionMode;
        if (input->inputKind == INPUT_KEY ) {
                enum KeyEventKind keyEventKind = input->data.tKey.keyEventKind;
                int hasCodepoint = input->data.tKey.hasCodepoint;
                /* !hasCodepoint currently means that the event came from GLFW's low-level
                key-Callback, and not the unicode Callback.
                We would like to use low-level access because that provides the modifiers,
                but unfortunately it doesn't respect keyboard layout, so we use high-level
                access (which doesn't provide modifiers). It's not clear at this point how
                we should handle combinations like Ctrl+Z while respecting keyboard layout.
                (There's a github issue for that). */
                if (hasCodepoint && (keyEventKind == KEYEVENT_PRESS || keyEventKind == KEYEVENT_RELEASE)) {
                        switch (input->data.tKey.codepoint) {
                        case 'j':
                                move_cursor_down(edit, isSelectionMode);
                                break;
                        case 'k':
                                move_cursor_up(edit, isSelectionMode);
                                break;
                        case 'h':
                                move_cursor_left(edit, isSelectionMode);
                                break;
                        case 'l':
                                move_cursor_right(edit, isSelectionMode);
                                break;
                        case '0':
                                move_cursor_to_beginning_of_line(edit, isSelectionMode);
                                break;
                        case '$':
                                move_cursor_to_end_of_line(edit, isSelectionMode);
                                break;
                        }
                }
                else if (!hasCodepoint && (keyEventKind == KEYEVENT_PRESS || keyEventKind == KEYEVENT_RELEASE)) {
                        int modifierMask = input->data.tKey.modifierMask;
                        switch (input->data.tKey.keyKind) {
                        case KEY_CURSORUP:
                                move_cursor_up(edit, isSelectionMode);
                                break;
                        case KEY_CURSORDOWN:
                                move_cursor_down(edit, isSelectionMode);
                                break;
                        case KEY_CURSORLEFT:
                                move_cursor_left(edit, isSelectionMode);
                                break;
                        case KEY_CURSORRIGHT:
                                move_cursor_right(edit, isSelectionMode);
                                break;
                        case KEY_PAGEUP:
                                scroll_up_one_page(edit, isSelectionMode);
                                break;
                        case KEY_PAGEDOWN:
                                scroll_down_one_page(edit, isSelectionMode);
                                break;
                        case KEY_HOME:
                                if (modifierMask & MODIFIER_CONTROL)
                                        move_cursor_to_first_line(edit, isSelectionMode);
                                else
                                        move_cursor_to_beginning_of_line(edit, isSelectionMode);
                                break;
                        case KEY_END:
                                if (modifierMask & MODIFIER_CONTROL)
                                        move_cursor_to_last_line(edit, isSelectionMode);
                                else
                                        move_cursor_to_end_of_line(edit, isSelectionMode);
                                break;
                        }
                }
        }
}

static void process_input_in_TextEdit_with_ViMode_in_VIMODE_NORMAL_MODAL_D(
        struct Input *input, struct TextEdit *edit, struct ViState *state)
{
        UNUSED(edit);
        if (input->inputKind == INPUT_KEY) {
                //enum KeyKind keyKind = input->data.tKey.keyKind;
                enum KeyEventKind keyEventKind = input->data.tKey.keyEventKind;
                int hasCodepoint = input->data.tKey.hasCodepoint;
                if (keyEventKind == KEYEVENT_PRESS || keyEventKind == KEYEVENT_RELEASE) {
                        if (hasCodepoint) {
                                switch (input->data.tKey.codepoint) {
                                case 'd':
                                        // not implemented yet: erase_line(edit);
                                        state->modalKind = VIMODAL_NORMAL;
                                        break;
                                default:
                                        // "delete with movement"
                                        break;
                                }
                        }
                }
        }
}

static void process_input_in_TextEdit_with_ViMode_in_VIMODE_NORMAL(
        struct Input *input, struct TextEdit *edit, struct ViState *state)
{
        if (edit->vistate.modalKind == VIMODAL_D) {
                process_input_in_TextEdit_with_ViMode_in_VIMODE_NORMAL_MODAL_D(input, edit, state);
                return;
        }

        if (input->inputKind == INPUT_KEY) {
                enum KeyEventKind keyEventKind = input->data.tKey.keyEventKind;
                int hasCodepoint = input->data.tKey.hasCodepoint;
                /* !hasCodepoint currently means that the event came from GLFW's low-level
                key-Callback, and not the unicode Callback.
                We would like to use low-level access because that provides the modifiers,
                but unfortunately it doesn't respect keyboard layout, so we use high-level
                access (which doesn't provide modifiers). It's not clear at this point how
                we should handle combinations like Ctrl+Z while respecting keyboard layout.
                (There's a github issue for that). */
                if (hasCodepoint && (keyEventKind == KEYEVENT_PRESS || keyEventKind == KEYEVENT_RELEASE)) {
                        switch (input->data.tKey.codepoint) {
                        case 'A':
                                move_cursor_to_end_of_line(edit, 0);
                                state->vimodeKind = VIMODE_INPUT;
                                break;
                        case 'I':
                                move_cursor_to_beginning_of_line(edit, 0);
                                state->vimodeKind = VIMODE_INPUT;
                                break;
                        case 'D':
                                // not implemented yet: delete_to_rest_of_line(edit, 0);
                                break;
                        case 'a':
                                move_cursor_right(edit, 0);
                                state->vimodeKind = VIMODE_INPUT;
                                break;
                        case 'i':
                                state->vimodeKind = VIMODE_INPUT;
                                break;
                        case 'd':
                                state->modalKind = VIMODAL_D;
                                break;
                        case 'v':
                                edit->isSelectionMode = 1;
                                edit->selectionStartBytePosition = edit->cursorBytePosition;
                                state->vimodeKind = VIMODE_SELECTING;
                                break;
                        case 'x':
                                erase_forwards_in_TextEdit(edit);
                                break;
                        case 'X':
                                erase_backwards_in_TextEdit(edit);
                                break;
                        default:
                                process_movements_in_ViMode_NORMAL_or_SELECTING(input, edit, state);
                                break;
                        }
                }
                else if (!hasCodepoint && (keyEventKind == KEYEVENT_PRESS || keyEventKind == KEYEVENT_RELEASE)) {
                        switch (input->data.tKey.keyKind) {
                        case KEY_DELETE:
                                erase_forwards_in_TextEdit(edit);
                                break;
                        case KEY_BACKSPACE:
                                move_cursor_left(edit, 0);  // compare with KEY_DELETE, doesn't delete. It's strange but VIM doesn it this way.
                                break;
                        default:
                                process_movements_in_ViMode_NORMAL_or_SELECTING(input, edit, state);
                                break;
                        }
                }
        }
}

static void process_input_in_TextEdit_with_ViMode_in_VIMODE_SELECTING(
        struct Input *input, struct TextEdit *edit, struct ViState *state)
{
        if (input->inputKind == INPUT_KEY) {
                enum KeyEventKind keyEventKind = input->data.tKey.keyEventKind;
                int hasCodepoint = input->data.tKey.hasCodepoint;
                /* !hasCodepoint currently means that the event came from GLFW's low-level
                key-Callback, and not the unicode Callback.
                We would like to use low-level access because that provides the modifiers,
                but unfortunately it doesn't respect keyboard layout, so we use high-level
                access (which doesn't provide modifiers). It's not clear at this point how
                we should handle combinations like Ctrl+Z while respecting keyboard layout.
                (There's a github issue for that). */
                if (hasCodepoint && (keyEventKind == KEYEVENT_PRESS || keyEventKind == KEYEVENT_RELEASE)) {
                        switch (input->data.tKey.codepoint) {
                        case 'x':
                        case 'X':
                        case 'd':
                        case 'D':
                                erase_selected_in_TextEdit(edit);
                                state->vimodeKind = VIMODE_NORMAL;
                                break;
                        default:
                                process_movements_in_ViMode_NORMAL_or_SELECTING(input, edit, state);
                                break;
                        }
                }
                else if (!hasCodepoint) {
                        switch (input->data.tKey.keyKind) {
                        case KEY_ESCAPE:
                                edit->isSelectionMode = 0;
                                state->vimodeKind = VIMODE_NORMAL;
                                break;
                        case KEY_BACKSPACE:
                        case KEY_DELETE:
                                erase_selected_in_TextEdit(edit);
                                state->vimodeKind = VIMODE_NORMAL;
                                break;
                        default:
                                process_movements_in_ViMode_NORMAL_or_SELECTING(input, edit, state);
                                break;
                        }
                }
        }
}

static void process_input_in_TextEdit_with_ViMode_in_VIMODE_INPUT(
        struct Input *input, struct TextEdit *edit, struct ViState *state)
{
        if (input->inputKind == INPUT_KEY) {
                enum KeyEventKind keyEventKind = input->data.tKey.keyEventKind;
                int hasCodepoint = input->data.tKey.hasCodepoint;
                if (!hasCodepoint && (keyEventKind == KEYEVENT_PRESS || keyEventKind == KEYEVENT_RELEASE)) {
                        int modifiers = input->data.tKey.modifierMask;
                        int isSelecting = modifiers & MODIFIER_SHIFT;  // only relevant for some inputs
                        switch (input->data.tKey.keyKind) {
                        case KEY_ENTER:
                                insert_codepoint_into_textedit(edit, 0x0a);
                                break;
                        case KEY_ESCAPE:
                                state->vimodeKind = VIMODE_NORMAL;
                                break;
                        case KEY_HOME:
                                if (modifiers & MODIFIER_CONTROL)
                                        move_cursor_to_first_line(edit, isSelecting);
                                else
                                        move_cursor_to_beginning_of_line(edit, isSelecting);
                                break;
                        case KEY_END:
                                if (modifiers & MODIFIER_CONTROL)
                                        move_cursor_to_last_line(edit, isSelecting);
                                else
                                        move_cursor_to_end_of_line(edit, isSelecting);
                                break;
                        case KEY_PAGEUP:
                                scroll_up_one_page(edit, isSelecting);
                                break;
                        case KEY_PAGEDOWN:
                                scroll_down_one_page(edit, isSelecting);
                                break;
                        case KEY_DELETE:
                                if (edit->isSelectionMode)
                                        erase_selected_in_TextEdit(edit);
                                else
                                        erase_forwards_in_TextEdit(edit);
                                break;
                        case KEY_BACKSPACE:
                                if (edit->isSelectionMode)
                                        erase_selected_in_TextEdit(edit);
                                else
                                        erase_backwards_in_TextEdit(edit);
                                break;
                        }
                }
                else if (hasCodepoint) {
                        unsigned long codepoint = input->data.tKey.codepoint;
                        insert_codepoint_into_textedit(edit, codepoint);
                }
        }
}

void process_input_in_TextEdit_with_ViMode(struct Input *input, struct TextEdit *edit, struct ViState *state)
{
        ENSURE(!edit->isLoading);

        if (state->vimodeKind == VIMODE_INPUT)
                process_input_in_TextEdit_with_ViMode_in_VIMODE_INPUT(input, edit, state);
        else if (state->vimodeKind == VIMODE_SELECTING)
                process_input_in_TextEdit_with_ViMode_in_VIMODE_SELECTING(input, edit, state);
        else if (state->vimodeKind == VIMODE_NORMAL)
                process_input_in_TextEdit_with_ViMode_in_VIMODE_NORMAL(input, edit, state);
}





void process_input_in_TextEdit(struct Input *input, struct TextEdit *edit)
{
        if (edit->isVimodeActive) {
                process_input_in_TextEdit_with_ViMode(input, edit, &edit->vistate);
                return;
        }

        ENSURE(!edit->isLoading);
        if (input->inputKind == INPUT_KEY) {
                int modifiers = input->data.tKey.modifierMask;
                int isSelecting = modifiers & MODIFIER_SHIFT;  // only relevant for some inputs
                switch (input->data.tKey.keyKind) {
                case KEY_ENTER:
                        insert_codepoint_into_textedit(edit, 0x0a);
                        break;
                case KEY_CURSORLEFT:
                        move_cursor_left(edit, isSelecting);
                        break;
                case KEY_CURSORRIGHT:
                        move_cursor_right(edit, isSelecting);
                        break;
                case KEY_CURSORUP:
                        move_cursor_up(edit, isSelecting);
                        break;
                case KEY_CURSORDOWN:
                        move_cursor_down(edit, isSelecting);
                        break;
                case KEY_HOME:
                        if (modifiers & MODIFIER_CONTROL)
                                move_cursor_to_first_line(edit, isSelecting);
                        else
                                move_cursor_to_beginning_of_line(edit, isSelecting);
                        break;
                case KEY_END:
                        if (modifiers & MODIFIER_CONTROL)
                                move_cursor_to_last_line(edit, isSelecting);
                        else
                                move_cursor_to_end_of_line(edit, isSelecting);
                        break;
                case KEY_PAGEUP:
                        scroll_up_one_page(edit, isSelecting);
                        break;
                case KEY_PAGEDOWN:
                        scroll_down_one_page(edit, isSelecting);
                        break;
                case KEY_DELETE:
                        if (edit->isSelectionMode)
                                erase_selected_in_TextEdit(edit);
                        else
                                erase_forwards_in_TextEdit(edit);
                        break;
                case KEY_BACKSPACE:
                        if (edit->isSelectionMode)
                                erase_selected_in_TextEdit(edit);
                        else
                                erase_backwards_in_TextEdit(edit);
                        break;
                default:
                        if (input->data.tKey.hasCodepoint) {
                                if (edit->isSelectionMode)
                                        erase_selected_in_TextEdit(edit);
                                unsigned long codepoint = input->data.tKey.codepoint;
                                insert_codepoint_into_textedit(edit, codepoint);
                                //debug_check_textrope(edit->rope);
                        }
                        break;
                }
        }
}
