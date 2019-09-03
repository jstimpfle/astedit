#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <astedit/vimode.h>
#include <astedit/eventhandling.h>


static int is_input_keypress(struct Input *input)
{
        return input->inputKind == INPUT_KEY
                && (input->data.tKey.keyEventKind == KEYEVENT_PRESS
                        || input->data.tKey.keyEventKind == KEYEVENT_REPEAT);
}

static int is_input_keypress_of_key(struct Input *input, enum KeyKind keyKind)
{
        return is_input_keypress(input) && input->data.tKey.keyKind == keyKind;
}

static int is_input_keypress_of_key_and_modifiers(struct Input *input, enum KeyKind keyKind, int modifierBits)
{
        return is_input_keypress_of_key(input, keyKind)
                && ((modifierBits & input->data.tKey.modifierMask)
                        == modifierBits);
}

static int is_input_unicode(struct Input *input)
{
        return input->inputKind == INPUT_KEY && input->data.tKey.hasCodepoint;
}

static int is_input_unicode_of_codepoint(struct Input *input, uint32_t codepoint)
{
        return is_input_unicode(input) && input->data.tKey.codepoint == codepoint;
}

static int input_to_movement_in_Vi(struct Input *input, struct Movement *outMovement)
{
        if (input->inputKind != INPUT_KEY)
                return 0;
        if (input->data.tKey.keyEventKind != KEYEVENT_PRESS
                && input->data.tKey.keyEventKind != KEYEVENT_REPEAT)
                return 0;
        struct Movement movement = {0}; // initialize to avoid compiler warning
        int badKey = 0;
        int modifierBits = input->data.tKey.modifierMask;
        switch (input->data.tKey.keyKind) {
                case KEY_CURSORLEFT:  movement = (struct Movement) { MOVEMENT_LEFT }; break;
                case KEY_CURSORRIGHT: movement = (struct Movement) { MOVEMENT_RIGHT }; break;
                case KEY_CURSORUP:    movement = (struct Movement) { MOVEMENT_UP }; break;
                case KEY_CURSORDOWN:  movement = (struct Movement) { MOVEMENT_DOWN }; break;
                case KEY_PAGEUP:      movement = (struct Movement) { MOVEMENT_PAGEUP }; break;
                case KEY_PAGEDOWN:    movement = (struct Movement) { MOVEMENT_PAGEDOWN }; break;
                case KEY_HOME:
                        if (modifierBits & MODIFIER_CONTROL)
                                movement = (struct Movement) { MOVEMENT_FIRSTLINE };
                        else
                                movement = (struct Movement) { MOVEMENT_LINEBEGIN };
                        break;
                case KEY_END:
                        if (modifierBits & MODIFIER_CONTROL)
                                movement = (struct Movement) { MOVEMENT_LASTLINE };
                        else
                                movement = (struct Movement) { MOVEMENT_LINEEND };
                        break;
                default:
                        badKey = 1;
        }
        if (badKey && input->data.tKey.hasCodepoint) {
                badKey = 0;
                switch (input->data.tKey.codepoint) {
                case '0': movement = (struct Movement) { MOVEMENT_LINEBEGIN }; break;
                case '$': movement = (struct Movement) { MOVEMENT_LINEEND }; break;
                case 'h': movement = (struct Movement) { MOVEMENT_LEFT }; break;
                case 'j': movement = (struct Movement) { MOVEMENT_DOWN }; break;
                case 'k': movement = (struct Movement) { MOVEMENT_UP }; break;
                case 'l': movement = (struct Movement) { MOVEMENT_RIGHT }; break;
                case 'G': movement = (struct Movement) { MOVEMENT_LASTLINE }; break;
                default:
                        badKey = 1;
                }
        }
        if (badKey)
                return 0;
        *outMovement = movement;
        return 1;
}

static void process_movements_in_ViMode_NORMAL_or_SELECTING(
        struct Input *input, struct TextEdit *edit, struct ViState *state)
{
        UNUSED(state);
        int isSelecting = edit->isSelectionMode;
        struct Movement movement;
        if (input_to_movement_in_Vi(input, &movement))
                move_cursor_with_movement(edit, &movement, isSelecting);
}

static void process_input_in_TextEdit_with_ViMode_in_VIMODE_NORMAL_MODAL_D(
        struct Input *input, struct TextEdit *edit, struct ViState *state)
{
        UNUSED(edit);
        if (input->inputKind == INPUT_KEY) {
                //enum KeyKind keyKind = input->data.tKey.keyKind;
                enum KeyEventKind keyEventKind = input->data.tKey.keyEventKind;
                int hasCodepoint = input->data.tKey.hasCodepoint;
                if (keyEventKind == KEYEVENT_PRESS || keyEventKind == KEYEVENT_REPEAT) {
                        struct Movement movement;
                        if (input_to_movement_in_Vi(input, &movement)) {
                                delete_with_movement(edit, &movement);
                                state->modalKind = VIMODAL_NORMAL;
                        }
                        else if (hasCodepoint) {
                                switch (input->data.tKey.codepoint) {
                                case 'd':
                                        delete_current_line(edit);
                                        state->modalKind = VIMODAL_NORMAL;
                                        break;
                                default:
                                        state->modalKind = VIMODAL_NORMAL;
                                        break;
                                }
                        }
                }
        }
}

static void process_input_in_TextEdit_with_ViMode_in_VIMODE_NORMAL_MODAL_G(
        struct Input *input, struct TextEdit *edit, struct ViState *state)
{
        if (input->inputKind == INPUT_KEY) {
                int isSelecting = edit->isSelectionMode;
                //enum KeyKind keyKind = input->data.tKey.keyKind;
                enum KeyEventKind keyEventKind = input->data.tKey.keyEventKind;
                int hasCodepoint = input->data.tKey.hasCodepoint;
                if (keyEventKind == KEYEVENT_PRESS || keyEventKind == KEYEVENT_REPEAT) {
                        if (hasCodepoint) {
                                switch (input->data.tKey.codepoint) {
                                case 'g':
                                        move_cursor_to_first_line(edit, isSelecting);
                                        state->modalKind = VIMODAL_NORMAL;
                                        break;
                                default:
                                        state->modalKind = VIMODAL_NORMAL;
                                        break;
                                }
                        }
                }
        }
}

static void process_input_in_TextEdit_with_ViMode_in_VIMODE_NORMAL(
        struct Input *input, struct TextEdit *edit, struct ViState *state)
{
        if (state->modalKind == VIMODAL_D) {
                process_input_in_TextEdit_with_ViMode_in_VIMODE_NORMAL_MODAL_D(input, edit, state);
                return;
        }
        if (state->modalKind == VIMODAL_G) {
                process_input_in_TextEdit_with_ViMode_in_VIMODE_NORMAL_MODAL_G(input, edit, state);
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
                if (hasCodepoint && (keyEventKind == KEYEVENT_PRESS || keyEventKind == KEYEVENT_REPEAT)) {
                        switch (input->data.tKey.codepoint) {
                        case ':':
                                reset_ViCmdline(&state->cmdline);
                                state->vimodeKind = VIMODE_COMMAND;
                                break;
                        case 'A':
                                move_cursor_to_end_of_line(edit, 0);
                                state->vimodeKind = VIMODE_INPUT;
                                break;
                        case 'I':
                                move_cursor_to_beginning_of_line(edit, 0);
                                state->vimodeKind = VIMODE_INPUT;
                                break;
                        case 'D':
                                delete_to_end_of_line(edit);
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
                        case 'g':
                                state->modalKind = VIMODAL_G;
                                break;
                        case 'o':
                                move_cursor_to_end_of_line(edit, 0);
                                if (edit->cursorBytePosition == textrope_length(edit->rope))
                                        insert_codepoint_into_textedit(edit, 0x0a);
                                move_cursor_to_next_codepoint(edit, 0);
                                insert_codepoint_into_textedit(edit, 0x0a);
                                state->vimodeKind = VIMODE_INPUT;
                                break;
                        case 'O':
                                move_cursor_to_beginning_of_line(edit, 0);
                                insert_codepoint_into_textedit(edit, 0x0a);
                                state->vimodeKind = VIMODE_INPUT;
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
                else if (!hasCodepoint && (keyEventKind == KEYEVENT_PRESS || keyEventKind == KEYEVENT_REPEAT)) {
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
        if (state->modalKind == VIMODAL_G) {
                process_input_in_TextEdit_with_ViMode_in_VIMODE_NORMAL_MODAL_G(input, edit, state);
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
                if (hasCodepoint && (keyEventKind == KEYEVENT_PRESS || keyEventKind == KEYEVENT_REPEAT)) {
                        switch (input->data.tKey.codepoint) {
                        case 'g':
                                state->modalKind = VIMODAL_G;
                                break;
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
                        int modifiers = input->data.tKey.modifierMask;
                        switch (input->data.tKey.keyKind) {
                        case KEY_C:
                                if (modifiers == MODIFIER_CONTROL) {
                                        edit->isSelectionMode = 0;
                                        state->vimodeKind = VIMODE_NORMAL;
                                }
                                break;
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
                if (!hasCodepoint && (keyEventKind == KEYEVENT_PRESS || keyEventKind == KEYEVENT_REPEAT)) {
                        int modifiers = input->data.tKey.modifierMask;
                        switch (input->data.tKey.keyKind) {
                        case KEY_C:
                                if (modifiers == MODIFIER_CONTROL)
                                        state->vimodeKind = VIMODE_NORMAL;
                                break;
                        case KEY_ENTER:
                                insert_codepoint_into_textedit(edit, 0x0a);
                                move_cursor_to_next_codepoint(edit, 0);
                                break;
                        case KEY_TAB:
                                for (int i = 0; i < 8; i++) {
                                        insert_codepoint_into_textedit(edit, ' ');
                                        move_cursor_to_next_codepoint(edit, 0);
                                }
                                break;
                        case KEY_ESCAPE:
                                state->vimodeKind = VIMODE_NORMAL;
                                break;
                        case KEY_DELETE:
                                erase_forwards_in_TextEdit(edit);
                                break;
                        case KEY_BACKSPACE:
                                erase_backwards_in_TextEdit(edit);
                                break;
                        }
                }
                else if (hasCodepoint) {
                        unsigned long codepoint = input->data.tKey.codepoint;
                        insert_codepoint_into_textedit(edit, codepoint);
                        move_cursor_to_next_codepoint(edit, 0);
                }
        }
}

static void process_input_in_TextEdit_with_ViMode_in_VIMODE_COMMAND(
        struct Input *input, struct TextEdit *edit, struct ViState *state)
{
        UNUSED(edit);
        struct ViCmdline *cmdline = &state->cmdline;  //XXX
        ENSURE(!cmdline->isAborted && !cmdline->isConfirmed);
        if (!is_input_keypress(input))
                return;
        if (is_input_unicode(input))
                insert_codepoint_in_ViCmdline(input->data.tKey.codepoint, cmdline);
        else if (is_input_keypress_of_key(input, KEY_BACKSPACE))
                erase_backwards_in_ViCmdline(cmdline);
        else if (is_input_keypress_of_key(input, KEY_DELETE))
                erase_forwards_in_ViCmdline(cmdline);
        else if (is_input_keypress_of_key(input, KEY_HOME))
                move_cursor_to_beginning_in_cmdline(cmdline);
        else if (is_input_keypress_of_key(input, KEY_END))
                move_cursor_to_end_in_cmdline(cmdline);
        else if (is_input_keypress_of_key(input, KEY_CURSORLEFT))
                move_cursor_left_in_cmdline(cmdline);
        else if (is_input_keypress_of_key(input, KEY_CURSORRIGHT))
                move_cursor_right_in_cmdline(cmdline);
        else if (is_input_keypress_of_key(input, KEY_ESCAPE))
                cmdline->isAborted = 1;
        else if (is_input_keypress_of_key(input, KEY_ENTER))
                cmdline->isConfirmed = 1;


        if (cmdline->isAborted) {
                // TODO
                reset_ViCmdline(cmdline);
                state->vimodeKind = VIMODE_NORMAL;
        }
        if (cmdline->isConfirmed) {
                // TODO
                interpret_cmdline(cmdline, edit);
                reset_ViCmdline(cmdline);
                state->vimodeKind = VIMODE_NORMAL;
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
        else if (state->vimodeKind == VIMODE_COMMAND)
                process_input_in_TextEdit_with_ViMode_in_VIMODE_COMMAND(input, edit, state);
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
                        move_cursor_to_next_codepoint(edit, isSelecting);
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
                                move_cursor_to_next_codepoint(edit, isSelecting);
                                //debug_check_textrope(edit->rope);
                        }
                        break;
                }
        }
}

void handle_input(struct Input *input, struct TextEdit *edit)
{
        if (input->inputKind == INPUT_WINDOWRESIZE) {
                log_postf("Window size is now %d %d",
                        input->data.tWindowresize.width,
                        input->data.tWindowresize.height);
        }
        else if (input->inputKind == INPUT_KEY) {
                if (input->data.tKey.keyKind == KEY_F4) {
                        //if (input->data.tKey.modifiers & MODIFIER_MOD)
                          //      shouldWindowClose = 1;
                }
                else if (is_input_keypress_of_key_and_modifiers(input, KEY_F11, MODIFIER_MOD)) {
                        toggle_fullscreen();
                }
                else if (!edit->isLoading) {
                        //start_timer(keyinputTimer);
                        process_input_in_TextEdit(input, edit);
                        //stop_timer(keyinputTimer);
                        /*report_timer(keyinputTimer, "Time spent in editing operation");*/
                }
        }
        else if (input->inputKind == INPUT_MOUSEBUTTON) {
                enum MousebuttonKind mousebuttonKind = input->data.tMousebutton.mousebuttonKind;
                enum MousebuttonEventKind mousebuttonEventKind = input->data.tMousebutton.mousebuttonEventKind;
                const char *event = mousebuttonEventKind == MOUSEBUTTONEVENT_PRESS? "Press" : "Release";
                static const char *const prefix[2] = { "with", "+" };
                int flag = 0;
                log_begin();
                log_writef("%s mouse button %d", event, mousebuttonKind);
                if (input->data.tMousebutton.modifiers & MODIFIER_CONTROL) {
                        log_writef(" %s %s", prefix[flag], "Ctrl");
                        flag = 1;
                }
                if (input->data.tMousebutton.modifiers & MODIFIER_MOD) {
                        log_writef(" %s %s", prefix[flag], "Mod");
                        flag = 1;
                }
                if (input->data.tMousebutton.modifiers & MODIFIER_SHIFT) {
                        log_writef(" %s %s", prefix[flag], "Shift");
                        flag = 1;
                }
                log_end();
        }
}