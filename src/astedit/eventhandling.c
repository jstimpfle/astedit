#include <astedit/astedit.h>
#include <astedit/editor.h>
#include <astedit/logging.h>
#include <astedit/vimode.h>
#include <astedit/gfx.h>  // gfx_toggle_srgb()
#include <astedit/edithistory.h>
#include <astedit/textedit.h>
#include <astedit/texteditsearch.h>
#include <astedit/textpositions.h>
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

static int is_input_keypress_of_key_and_modifiers(struct Input *input, enum KeyKind keyKind, int modifiers)
{
        return is_input_keypress_of_key(input, keyKind)
                && ((modifiers & input->data.tKey.modifierMask)
                        == modifiers);
}

static int is_input_unicode(struct Input *input)
{
        return input->inputKind == INPUT_KEY && input->data.tKey.hasCodepoint;
}

/*
static int is_input_unicode_of_codepoint(struct Input *input, uint32_t codepoint)
{
        return is_input_unicode(input) && input->data.tKey.codepoint == codepoint;
}
*/

static void go_to_major_mode_in_vi(struct ViState *vi, enum ViMode vimodeKind)
{
        vi->vimodeKind = vimodeKind;
        vi->modalKind = VIMODAL_NORMAL;
        vi->moveModalKind = VIMOVEMODAL_NONE;
}

static void go_to_normal_mode_modal_in_vi(struct ViState *vi, enum ViNormalModeModal modalKind)
{
        ENSURE(vi->vimodeKind == VIMODE_NORMAL);
        vi->modalKind = modalKind;
}

static void go_to_move_modal_mode_in_vi(struct ViState *vi, enum ViMoveModal moveModalKind)
{
        //log_postf("going to move modal mode #%d", (int) moveModalKind);
        vi->moveModalKind = moveModalKind;
}

static struct {
        int keyKind;
        int modifiers;
        int movementKind;
} viMovementSimpleTable[] = {
        { KEY_CURSORLEFT, 0, MOVEMENT_LEFT },
        { KEY_CURSORRIGHT, 0, MOVEMENT_RIGHT },
        { KEY_CURSORLEFT, MODIFIER_CONTROL, MOVEMENT_PREVIOUS_WORD },
        { KEY_CURSORRIGHT, MODIFIER_CONTROL, MOVEMENT_NEXT_WORD },
        { KEY_CURSORUP, 0, MOVEMENT_UP },
        { KEY_CURSORDOWN, 0, MOVEMENT_DOWN },
        { KEY_D, MODIFIER_CONTROL, MOVEMENT_PAGEDOWN },
        { KEY_U, MODIFIER_CONTROL, MOVEMENT_PAGEUP },
        { KEY_PAGEUP, 0, MOVEMENT_PAGEUP },
        { KEY_PAGEDOWN, 0, MOVEMENT_PAGEDOWN },
        { KEY_HOME, 0, MOVEMENT_LINEBEGIN },
        { KEY_HOME, MODIFIER_CONTROL, MOVEMENT_FIRSTLINE },
        { KEY_END, 0, MOVEMENT_LINEEND },
        { KEY_END, MODIFIER_CONTROL, MOVEMENT_LASTLINE },
        { KEY_F3, 0, MOVEMENT_NEXT_MATCH },
};

/* NOTE! due to technical problems we are currently not able to receive any
 * codepoints with modifier bits. The shift character is received implicitly
 * for "keys" that have an uppercase version. Though that might be layout
 * specific! */
static struct {
        int codepoint;
        int movementKind;
} viMovementCodepointTable[] = {
        { '0', MOVEMENT_LINEBEGIN },
        { '$', MOVEMENT_LINEEND },
        { 'h', MOVEMENT_LEFT },
        { 'j', MOVEMENT_DOWN },
        { 'k', MOVEMENT_UP },
        { 'l', MOVEMENT_RIGHT },
        { 'G', MOVEMENT_LASTLINE },
        { 'w', MOVEMENT_NEXT_WORD },
        { 'b', MOVEMENT_PREVIOUS_WORD },
        { 'n', MOVEMENT_NEXT_MATCH },
};

static int input_to_movement_in_Vi(struct Input *input, struct Movement *outMovement)
{
        if (input->inputKind != INPUT_KEY)
                return 0;
        if (input->data.tKey.keyEventKind != KEYEVENT_PRESS
                && input->data.tKey.keyEventKind != KEYEVENT_REPEAT)
                return 0;
        struct Movement movement = {0}; // initialize to avoid compiler warning
        int keyKind = input->data.tKey.keyKind;
        int modifiers = input->data.tKey.modifierMask;
        int hasCodepoint = input->data.tKey.hasCodepoint;
        int codepoint = (int) input->data.tKey.codepoint;
        for (int i = 0; i < LENGTH(viMovementSimpleTable); i++) {
                if (viMovementSimpleTable[i].keyKind == keyKind
                    && viMovementSimpleTable[i].modifiers == modifiers) {
                        movement = (struct Movement) { viMovementSimpleTable[i].movementKind };
                        goto good;
                }
        }
        if (hasCodepoint) {
                for (int i = 0; i < LENGTH(viMovementCodepointTable); i++) {
                        if (viMovementCodepointTable[i].codepoint == codepoint) {
                                movement = (struct Movement) { viMovementCodepointTable[i].movementKind };
                                goto good;
                        }
                }
        }
        return 0;
good:
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

static int do_movemodal_in_vi(struct Input *input, struct TextEdit *edit, struct ViState *state,
                               struct Movement *outMovement)
{
        UNUSED(edit);
        if (is_input_keypress(input)) {
                switch (state->moveModalKind) {
                case VIMOVEMODAL_G:
                {
                        //int isSelecting = edit->isSelectionMode;
                        //enum KeyKind keyKind = input->data.tKey.keyKind;
                        int hasCodepoint = input->data.tKey.hasCodepoint;
                        if (hasCodepoint) {
                                switch (input->data.tKey.codepoint) {
                                case 'g':
                                        // TODO: use "isSelecting"
                                        state->moveModalKind = VIMOVEMODAL_NONE;
                                        *outMovement = (struct Movement) { MOVEMENT_FIRSTLINE };
                                        return 1;
                                default:
                                        state->moveModalKind = VIMOVEMODAL_NONE;
                                        //XXX need "no movement"
                                        *outMovement = (struct Movement) { MOVEMENT_RIGHT };
                                        return 1;
                                }
                        }
                        break;
                }
                case VIMOVEMODAL_T:
                        state->moveModalKind = VIMOVEMODAL_NONE;
                        log_postf("VIMOVEMODAL_T not yet implemented.");
                        //XXX need "no movement"
                        *outMovement = (struct Movement) { MOVEMENT_RIGHT };
                        return 1;
                case VIMOVEMODAL_F:
                        log_postf("VIMOVEMODAL_F not yet implemented.");
                        //XXX need "no movement"
                        *outMovement = (struct Movement) { MOVEMENT_RIGHT };
                        return 1;
                default:
                        break;
                }
        }
        return 0;
}

static int maybe_start_movemodal_in_vi(struct Input *input, struct TextEdit *edit, struct ViState *state)
{
        UNUSED(edit);
        if (is_input_keypress(input)) {
                if (input->data.tKey.hasCodepoint) {
                        switch (input->data.tKey.codepoint) {
                        case 't':
                                go_to_move_modal_mode_in_vi(state, VIMOVEMODAL_T);
                                break;
                        case 'f':
                                go_to_move_modal_mode_in_vi(state, VIMOVEMODAL_F);
                                break;
                        case 'g':
                                go_to_move_modal_mode_in_vi(state, VIMOVEMODAL_G);
                                break;
                        default:
                                return 0;
                        }
                        return 1;
                }
        }
        return 0;
}

static void process_input_in_TextEdit_with_ViMode_in_rangeoperation_mode(
        struct Input *input, struct TextEdit *edit, struct ViState *state)
{
        // either delete or "change" a.k.a replace
        int isReplace = state->modalKind == VIMODAL_RANGEOPERATION_REPLACE;

        if (state->moveModalKind != VIMOVEMODAL_NONE) {
                struct Movement modalMovement;
                if (do_movemodal_in_vi(input, edit, state, &modalMovement)) {
                        delete_with_movement(edit, &modalMovement);
                        if (isReplace) {
                                go_to_major_mode_in_vi(state, VIMODE_INPUT);
                        }
                        else {
                                go_to_normal_mode_modal_in_vi(state, VIMODAL_NORMAL);
                        }
                }
                return;
        }

        UNUSED(edit);
        if (is_input_keypress(input)) {
                //enum KeyKind keyKind = input->data.tKey.keyKind;
                int hasCodepoint = input->data.tKey.hasCodepoint;
                struct Movement movement;
                if (input_to_movement_in_Vi(input, &movement)) {
                        delete_with_movement(edit, &movement);
                        if (isReplace) {
                                go_to_major_mode_in_vi(state, VIMODE_INPUT);
                        }
                        else {
                                go_to_normal_mode_modal_in_vi(state, VIMODAL_NORMAL);
                        }
                }
                else if (hasCodepoint) {
                        switch (input->data.tKey.codepoint) {
                        case 'd':
                                delete_current_line(edit);
                                go_to_major_mode_in_vi(state, VIMODE_NORMAL);
                                break;
                        default:
                                if (!maybe_start_movemodal_in_vi(input, edit, state))
                                        go_to_major_mode_in_vi(state, VIMODE_NORMAL);
                                break;
                        }
                }
        }
}

static void process_input_in_TextEdit_with_ViMode_in_VIMODE_NORMAL(
        struct Input *input, struct TextEdit *edit, struct ViState *state)
{
        /* XXX TODO RANGEOPERATION vs MOVEMODAL */
        if (state->modalKind == VIMODAL_RANGEOPERATION_REPLACE
            || state->modalKind == VIMODAL_RANGEOPERATION_DELETE) {
                process_input_in_TextEdit_with_ViMode_in_rangeoperation_mode(input, edit, state);
                return;
        }

        if (state->moveModalKind != VIMOVEMODAL_NONE) {
                struct Movement modalMovement;
                if (do_movemodal_in_vi(input, edit, state, &modalMovement)) {
                        int isSelecting = 0;
                        move_cursor_with_movement(edit, &modalMovement, isSelecting);
                }
                return;
        }

        if (is_input_keypress(input)) {
                if (edit->haveNotification)
                        // clear notification
                        edit->haveNotification = 0;

                int hasCodepoint = input->data.tKey.hasCodepoint;
                int modifiers = input->data.tKey.modifierMask;
                /* !hasCodepoint currently means that the event came from GLFW's low-level
                key-Callback, and not the unicode Callback.
                We would like to use low-level access because that provides the modifiers,
                but unfortunately it doesn't respect keyboard layout, so we use high-level
                access (which doesn't provide modifiers). It's not clear at this point how
                we should handle combinations like Ctrl+Z while respecting keyboard layout.
                (There's a github issue for that). */
                if (hasCodepoint) {
                        switch (input->data.tKey.codepoint) {
                        case ':':
                                clear_ViCmdline(&state->cmdline);
                                go_to_major_mode_in_vi(state, VIMODE_COMMAND);
                                break;
                        case 'A':
                                move_cursor_to_end_of_line(edit, 0);
                                go_to_major_mode_in_vi(state, VIMODE_INPUT);
                                break;
                        case 'I':
                                move_cursor_to_beginning_of_line(edit, 0);
                                go_to_major_mode_in_vi(state, VIMODE_INPUT);
                                break;
                        case 'D':
                                delete_to_end_of_line(edit);
                                break;
                        case 'a':
                                move_cursor_right(edit, 0);
                                go_to_major_mode_in_vi(state, VIMODE_INPUT);
                                break;
                        case 'i':
                                go_to_major_mode_in_vi(state, VIMODE_INPUT);
                                break;
                        /* XXX TODO clean up here. We should use
                         * move_modal stuff for c,d,f,t,g,... and handle things from
                         * there. */
                        case 'c':
                                go_to_normal_mode_modal_in_vi(state, VIMODAL_RANGEOPERATION_REPLACE);
                                break;
                        case 'd':
                                go_to_normal_mode_modal_in_vi(state, VIMODAL_RANGEOPERATION_DELETE);
                                break;
                        case 'g':
                                go_to_move_modal_mode_in_vi(state, VIMOVEMODAL_G);
                                break;
                        case 'o':
                                move_cursor_to_end_of_line(edit, 0);
                                if (edit->cursorBytePosition == textrope_length(edit->rope))
                                        insert_codepoint_into_textedit(edit, 0x0a);
                                move_cursor_to_next_codepoint(edit, 0);
                                insert_codepoint_into_textedit(edit, 0x0a);
                                go_to_major_mode_in_vi(state, VIMODE_INPUT);
                                break;
                        case 'O':
                                move_cursor_to_beginning_of_line(edit, 0);
                                insert_codepoint_into_textedit(edit, 0x0a);
                                go_to_major_mode_in_vi(state, VIMODE_INPUT);
                                break;
                        case 'r':
                                if (modifiers & MODIFIER_CONTROL)
                                        redo_next_edit_operation(edit);
                                break;
                        case 'u':
                                undo_last_edit_operation(edit);
                                break;
                        case 'v':
                                /*XXX need clean way to enable selection mode */
                                move_cursor_to_next_codepoint(edit, 1);
                                break;
                        case 'x':
                                delete_to_next_codepoint(edit);
                                break;
                        case 'X':
                                delete_to_previous_codepoint(edit);
                                break;
                        default:
                                process_movements_in_ViMode_NORMAL_or_SELECTING(input, edit, state);
                                break;
                        }
                }
                else if (!hasCodepoint) {
                        switch (input->data.tKey.keyKind) {
                        case KEY_DELETE:
                                delete_to_next_codepoint(edit);
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
        if (state->moveModalKind != VIMOVEMODAL_NONE) {
                struct Movement modalMovement;
                if (do_movemodal_in_vi(input, edit, state, &modalMovement)) {
                        int isSelecting = 1;
                        move_cursor_with_movement(edit, &modalMovement, isSelecting);
                }
                return;
        }

        if (is_input_keypress(input)) {
                int hasCodepoint = input->data.tKey.hasCodepoint;
                int modifiers = input->data.tKey.modifierMask;
                /* !hasCodepoint currently means that the event came from GLFW's low-level
                key-Callback, and not the unicode Callback.
                We would like to use low-level access because that provides the modifiers,
                but unfortunately it doesn't respect keyboard layout, so we use high-level
                access (which doesn't provide modifiers). It's not clear at this point how
                we should handle combinations like Ctrl+Z while respecting keyboard layout.
                (There's a github issue for that). */
                /* Update 2020-01, the X11 backend now gives the events a little
                 * differently. I still don't know how to handle the situation,
                 * but for now I'll ocnsider the unicode codepoint if there is
                 * no modifer and otherwise I'll consider the KEY_ vlaue */
                if (hasCodepoint) {
                        switch (input->data.tKey.codepoint) {
                        case 'c':
                                if (modifiers == MODIFIER_CONTROL) {
                                        edit->isSelectionMode = 0;
                                        go_to_major_mode_in_vi(state, VIMODE_NORMAL);
                                }
                                break;
                        case 'g':
                                go_to_move_modal_mode_in_vi(state, VIMOVEMODAL_G);
                                break;
                        case 'x':
                        case 'X':
                        case 'd':
                        case 'D':
                                erase_selected_in_TextEdit(edit);
                                go_to_major_mode_in_vi(state, VIMODE_NORMAL);
                                break;
                        default:
                                process_movements_in_ViMode_NORMAL_or_SELECTING(input, edit, state);
                                break;
                        }
                }
                else {
                        switch (input->data.tKey.keyKind) {
                        case KEY_ESCAPE:
                                edit->isSelectionMode = 0;
                                go_to_major_mode_in_vi(state, VIMODE_NORMAL);
                                break;
                        case KEY_BACKSPACE:
                        case KEY_DELETE:
                                erase_selected_in_TextEdit(edit);
                                go_to_major_mode_in_vi(state, VIMODE_NORMAL);
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
        if (is_input_keypress(input)) {
                int hasCodepoint = input->data.tKey.hasCodepoint;
                int modifiers = input->data.tKey.modifierMask;
                if (hasCodepoint) {
                        uint32_t codepoint = input->data.tKey.codepoint;
                        if (codepoint == 'c' && (modifiers & MODIFIER_CONTROL))
                                go_to_major_mode_in_vi(state, VIMODE_NORMAL);
                        else {
                                unsigned long codepoint = input->data.tKey.codepoint;
                                insert_codepoint_into_textedit(edit, codepoint);
                                move_cursor_to_next_codepoint(edit, 0);
                        }
                }
                else {
                        switch (input->data.tKey.keyKind) {
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
                                go_to_major_mode_in_vi(state, VIMODE_NORMAL);
                                break;
                        case KEY_DELETE:
                                delete_to_next_codepoint(edit);
                                break;
                        case KEY_BACKSPACE:
                                if (modifiers == MODIFIER_CONTROL)
                                        delete_with_movement(edit, MOVEMENT(MOVEMENT_PREVIOUS_WORD));
                                else
                                        delete_to_previous_codepoint(edit);
                                break;
                        }
                }
        }
}

static int is_cmdline_worthy_of_interpreting(struct ViCmdline *cmdline)
{
        for (int i = 0; i < cmdline->fill; i++)
                if (cmdline->buf[i] > 32) //XXX
                        return 1;
        return 0;
}

static void process_input_in_TextEdit_with_ViMode_in_VIMODE_COMMAND(
        struct Input *input, struct TextEdit *edit, struct ViState *state)
{
        if (!is_input_keypress(input))
                return;
        UNUSED(edit);
        struct ViCmdline *cmdline = &state->cmdline;  //XXX
        struct CmdlineHistory *history = &cmdline->history; //XXX
        ENSURE(!cmdline->isAborted && !cmdline->isConfirmed);
        if (is_input_keypress_of_key_and_modifiers(input, KEY_C, MODIFIER_CONTROL))
                cmdline->isAborted = 1;
        else if (is_input_keypress_of_key(input, KEY_ESCAPE))
                cmdline->isAborted = 1;
        else if (is_input_keypress_of_key(input, KEY_ENTER)) {
                // if still navigating, then first apply the currently "hovered"
                // cmdline
                if (cmdline->isNavigatingHistory) {
                        struct RememberedCmdline *updatedItem = cmdline->history.iter;
                        set_ViCmdline_contents_from_string(cmdline, updatedItem->cmdline, updatedItem->length);
                }
                cmdline->isConfirmed = 1;
        }
        else if (is_input_unicode(input) ||
                 is_input_keypress_of_key(input, KEY_BACKSPACE) ||
                 is_input_keypress_of_key(input, KEY_DELETE) ||
                 is_input_keypress_of_key(input, KEY_HOME) ||
                 is_input_keypress_of_key(input, KEY_END) ||
                 is_input_keypress_of_key(input, KEY_CURSORLEFT) ||
                 is_input_keypress_of_key(input, KEY_CURSORRIGHT)) {
                if (cmdline->isNavigatingHistory) {
                        cmdline->isNavigatingHistory = 0;
                        struct RememberedCmdline *updatedItem = cmdline->history.iter;
                        set_ViCmdline_contents_from_string(cmdline, updatedItem->cmdline, updatedItem->length);
                }
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
                else
                        ENSURE(0);
        }
        else if (cmdline->isNavigatingHistory) {
                if (is_input_keypress_of_key(input, KEY_CURSORDOWN)) {
                        struct RememberedCmdline *updatedItem;
                        updatedItem = go_to_next_cmdline(history);
                        if (updatedItem == NULL) {
                                cmdline->isNavigatingHistory = 0;
                        }
                }
                else if (is_input_keypress_of_key(input, KEY_CURSORUP))
                        go_to_previous_cmdline(history);
        }
        else if (!cmdline->isNavigatingHistory && is_input_keypress_of_key(input, KEY_CURSORUP)) {
                struct RememberedCmdline *updatedItem;
                if ((updatedItem = go_to_most_recent(&cmdline->history)) != NULL) {
                        cmdline->isNavigatingHistory = 1;
                }
        }

        if (cmdline->isAborted) {
                // TODO
                clear_ViCmdline(cmdline);
                go_to_major_mode_in_vi(state, VIMODE_NORMAL);
        }
        if (cmdline->isConfirmed) {
                // TODO
                if (is_cmdline_worthy_of_interpreting(cmdline)) {
                        interpret_cmdline(cmdline, edit);
                        add_to_cmdline_history(&cmdline->history, cmdline->buf, cmdline->fill);
                }
                clear_ViCmdline(cmdline);
                go_to_major_mode_in_vi(state, VIMODE_NORMAL);
        }
}

void process_input_in_TextEdit_with_ViMode(struct Input *input, struct TextEdit *edit, struct ViState *state)
{
        ENSURE(!edit->loading.isActive);
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
        if (edit->loading.isActive)
                return;

        if (is_input_keypress_of_key_and_modifiers(input, KEY_N, MODIFIER_CONTROL)) {
                globalData.isShowingLineNumbers ^= 1;
                return;
        }

        if (edit->isVimodeActive) {
                process_input_in_TextEdit_with_ViMode(input, edit, &edit->vistate);
                return;
        }

        // below code is bit-rotting since I've only use Vimode for some time...

        ENSURE(!edit->loading.isActive);
        if (input->inputKind == INPUT_KEY) {
                int modifiers = input->data.tKey.modifierMask;
                int isSelecting = modifiers & MODIFIER_SHIFT;  // only relevant for some inputs
                switch (input->data.tKey.keyKind) {
                case KEY_F3:
                        move_cursor_to_next_match(edit, isSelecting);
                        break;
                case KEY_ENTER:
                        insert_codepoint_into_textedit(edit, 0x0a);
                        move_cursor_to_next_codepoint(edit, isSelecting);
                        break;
                case KEY_CURSORLEFT:
                        if (modifiers == MODIFIER_CONTROL)
                                move_cursor_to_previous_word(edit, isSelecting);
                        else
                                move_cursor_left(edit, isSelecting);
                        break;
                case KEY_CURSORRIGHT:
                        if (modifiers == MODIFIER_CONTROL)
                                move_cursor_to_next_word(edit, isSelecting);
                        else
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
                        move_cursor_up_one_page(edit, isSelecting);
                        break;
                case KEY_PAGEDOWN:
                        move_cursor_down_one_page(edit, isSelecting);
                        break;
                case KEY_DELETE:
                        if (edit->isSelectionMode)
                                erase_selected_in_TextEdit(edit);
                        else
                                delete_to_next_codepoint(edit);
                        break;
                case KEY_BACKSPACE:
                        if (edit->isSelectionMode)
                                erase_selected_in_TextEdit(edit);
                        else
                                delete_to_previous_codepoint(edit);
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

void process_input_in_LineEdit(struct Input *input, struct LineEdit *lineEdit)
{
        static struct {
                int keyKind;
                void (*func)(struct LineEdit *lineEdit);
        } map[] = {
                { KEY_BACKSPACE, LineEdit_erase_backwards },
                { KEY_DELETE, LineEdit_erase_forwards },
                { KEY_HOME, LineEdit_move_cursor_to_beginning },
                { KEY_END, LineEdit_move_cursor_to_end },
                { KEY_CURSORLEFT, LineEdit_move_cursor_left },
                { KEY_CURSORRIGHT, LineEdit_move_cursor_right },
        };

        for (int i = 0; i < LENGTH(map); i++) {
                if (is_input_keypress_of_key(input, map[i].keyKind)) {
                        map[i].func(lineEdit);
                        return;
                }
        }
        if (is_input_unicode(input))
                LineEdit_insert_codepoint(input->data.tKey.codepoint, lineEdit);
}

void process_input_in_buffer_list_dialog(struct Input *input)
{
        struct ListSelect *list = &globalData.bufferSelect;

        if (is_input_keypress_of_key_and_modifiers(input, KEY_F, MODIFIER_CONTROL)) {
                list->isFilterActive ^= 1;
                return;
        }

        if (list->isFilterActive) {
                process_input_in_LineEdit(input, &list->filterLineEdit);
                list->isFilterRegexValid =
                        compile_regex_from_pattern(&list->filterRegex,
                                                   list->filterLineEdit.buf,
                                                   list->filterLineEdit.fill);
                ListSelect_select_first_matching_if_filter_does_not_match(list);
                // XXX currently falling through: trying all input processing
        }


        static struct {
                int keyKind;
                int actionKind;  // LISTSELECT_ACTION_???
        } map[] = {
                { KEY_CURSORDOWN, LISTSELECT_ACTION_MOVE_TO_NEXT },
                { KEY_CURSORUP, LISTSELECT_ACTION_MOVE_TO_PREV },
                /* currently disabled: this should not conflict with entering
                text into the filter box. */
                //{ KEY_J, LISTSELECT_ACTION_MOVE_TO_NEXT },
                //{ KEY_K, LISTSELECT_ACTION_MOVE_TO_PREV },
                { KEY_ENTER, LISTSELECT_ACTION_CONFIRM_SELECTION },
                { KEY_ESCAPE, LISTSELECT_ACTION_CANCEL_DIALOG },
        };

        for (int i = 0; i < LENGTH(map); i++) {
                if (is_input_keypress_of_key(input, map[i].keyKind)) {
                        ListSelect_do(list, map[i].actionKind);
                        break;
                }
        }
}

#include <string.h>
void handle_input(struct Input *input)
{
        if (is_input_keypress_of_key_and_modifiers(input, KEY_B, MODIFIER_CONTROL)) {
                globalData.isSelectingBuffer ^= 1;
                struct ListSelect *list = &globalData.bufferSelect;
                if (globalData.isSelectingBuffer) {
                        setup_ListSelect(list);
                        list->isFilterActive = 1; // good idea to always enable filter? I guess it's quicker to use it like this...
                        for (struct Buffer *buffer = buffers;
                             buffer != NULL; buffer = buffer->next) {
                                ListSelect_append_elem(list, buffer->name,
                                                       (int) strlen(buffer->name), buffer);
                        }
                }
                else {
                        teardown_ListSelect(list);
                }
                return;
        }

        if (globalData.isSelectingBuffer) {
                struct ListSelect *list = &globalData.bufferSelect;
                process_input_in_buffer_list_dialog(input);
                if (list->isConfirmed) {
                        int idx = list->selectedElemIndex;
                        ENSURE(0 <= idx && idx < list->numElems);
                        struct Buffer *buf = list->elems[idx].data;
                        switch_to_buffer(buf);
                        teardown_ListSelect(list);
                        globalData.isSelectingBuffer = 0;
                }
                if (list->isCancelled) {
                        teardown_ListSelect(list);
                        globalData.isSelectingBuffer = 0;
                }
        }
        else {
                process_input_in_TextEdit(input, activeTextEdit);
        }

        // also, do this other stuff here

        if (input->inputKind == INPUT_WINDOWRESIZE) {
                windowWidthInPixels = input->data.tWindowresize.width;
                windowHeightInPixels = input->data.tWindowresize.height;
        }
        else if (input->inputKind == INPUT_KEY) {
                if (is_input_keypress_of_key(input, KEY_F4)) {
                        //if (input->data.tKey.modifiers & MODIFIER_MOD)
                          //      shouldWindowClose = 1;
                }
                else if (is_input_keypress_of_key(input, KEY_F5)) {
                        gfx_toggle_srgb();
                }
                else if (is_input_keypress_of_key_and_modifiers(input, KEY_F11, MODIFIER_MOD)) {
                        toggle_fullscreen();
                }
        }
        else if (input->inputKind == INPUT_MOUSEBUTTON) {
                enum MousebuttonKind mousebuttonKind = input->data.tMousebutton.mousebuttonKind;
                enum MousebuttonEventKind mousebuttonEventKind = input->data.tMousebutton.mousebuttonEventKind;
                const char *event = mousebuttonEventKind == MOUSEBUTTONEVENT_PRESS ? "Press" : "Release";
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
