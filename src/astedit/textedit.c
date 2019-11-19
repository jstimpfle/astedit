#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/utf8.h>
#include <astedit/logging.h>
#include <astedit/osthread.h>
#include <astedit/textrope.h>
#include <astedit/textedit.h>
#include <astedit/texteditloadsave.h>
#include <astedit/edithistory.h>
#include <blunt/lex.h> /* test */
#include <string.h> // strlen()


enum {
        LINES_PER_PAGE = 15,   // XXX this value should be dependent on the current GUI viewport probably.
};

void insert_text_into_textedit(struct TextEdit *edit, FILEPOS insertPos, const char *text, FILEPOS length,
                                      FILEPOS nextCursorPosition)
{
        FILEPOS previousCursorPosition = edit->cursorBytePosition;
        insert_text_into_textrope(edit->rope, insertPos, text, length);
        record_insert_operation(edit, insertPos, length, previousCursorPosition, nextCursorPosition);
}

static void erase_text_from_textedit(struct TextEdit *edit, FILEPOS start, FILEPOS length)
{
        FILEPOS previousCursorPosition = edit->cursorBytePosition;
        FILEPOS nextCursorPosition = start; // TODO: Not sure that this is true generally.
        record_delete_operation(edit, start, length, previousCursorPosition, nextCursorPosition);
        erase_text_from_textrope(edit->rope, start, length);
}

static void setup_LinescrollAnimation(struct LinescrollAnimation *scrollAnimation)
{
        scrollAnimation->isActive = 0;
}

static void teardown_LinescrollAnimation(struct LinescrollAnimation *scrollAnimation)
{
        UNUSED(scrollAnimation);
}

static void start_LinescrollAnimation(struct LinescrollAnimation *scrollAnimation, FILEPOS startLine, FILEPOS targetLine)
{
        scrollAnimation->isActive = 1;
        scrollAnimation->startLine = startLine;
        scrollAnimation->targetLine = targetLine;
        scrollAnimation->progress = 0.0f;
        start_timer(&scrollAnimation->timer);
}

static void update_LinescrollAnimation(struct LinescrollAnimation *scrollAnimation)
{
        scrollAnimation->progress += 0.2f; //XXX
        if (scrollAnimation->progress >= 1.0f) {
                scrollAnimation->isActive = 0;
                stop_timer(&scrollAnimation->timer);
                //report_timer(edit->animationTimer, "Animation");
        }
}

static void make_range(FILEPOS a, FILEPOS b, FILEPOS *outStart, FILEPOS *outEnd)
{
        if (a < b) {
                *outStart = a;
                *outEnd = b;
        }
        else {
                *outStart = b;
                *outEnd = a;
        };
}

void get_selected_range_in_bytes(struct TextEdit *edit, FILEPOS *outStart, FILEPOS *outOnePastEnd)
{
        ENSURE(edit->isSelectionMode);
        make_range(edit->cursorBytePosition, edit->selectionStartBytePosition,
                outStart, outOnePastEnd);
}

void get_selected_range_in_codepoints(struct TextEdit *edit, FILEPOS *outStart, FILEPOS *outOnePastEnd)
{
        FILEPOS start;
        FILEPOS onePastEnd;
        get_selected_range_in_bytes(edit, &start, &onePastEnd);
        *outStart = compute_codepoint_position(edit->rope, start);
        *outOnePastEnd = compute_codepoint_position(edit->rope, onePastEnd);
}

void move_view_minimally_to_display_line(struct TextEdit *edit, FILEPOS lineNumber)
{
        FILEPOS targetLine;
        if (edit->firstLineDisplayed > lineNumber)
                targetLine = lineNumber;
        else if (edit->firstLineDisplayed < lineNumber - edit->numberOfLinesDisplayed + 1)
                targetLine = lineNumber - edit->numberOfLinesDisplayed + 1;
        else
                return;
        FILEPOS startLine = edit->firstLineDisplayed;
        start_LinescrollAnimation(&edit->scrollAnimation, startLine, targetLine);
        edit->firstLineDisplayed = targetLine;
}

void move_view_minimally_to_display_cursor(struct TextEdit *edit)
{
        FILEPOS lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
        move_view_minimally_to_display_line(edit, lineNumber);
}

void move_cursor_to_byte_position(struct TextEdit *edit, FILEPOS pos, int isSelecting)
{
        if (!edit->isSelectionMode && isSelecting)
                edit->selectionStartBytePosition = edit->cursorBytePosition;
        edit->isSelectionMode = isSelecting;
        if (isSelecting && edit->isVimodeActive)
                if (edit->vistate.vimodeKind != VIMODE_SELECTING)
                        edit->vistate.vimodeKind = VIMODE_SELECTING;
        edit->cursorBytePosition = pos;
        move_view_minimally_to_display_cursor(edit);
}

void move_cursor_to_codepoint(struct TextEdit *edit, FILEPOS codepointPos, int isSelecting)
{
        FILEPOS pos = compute_pos_of_codepoint(edit->rope, codepointPos);
        move_cursor_to_byte_position(edit, pos, isSelecting);
}

FILEPOS get_position_next_codepoint(struct TextEdit *edit)
{
        FILEPOS totalCodepoints = textrope_number_of_codepoints(edit->rope);
        FILEPOS codepointPos = compute_codepoint_position(edit->rope, edit->cursorBytePosition);
        if (codepointPos < totalCodepoints)
                return compute_pos_of_codepoint(edit->rope, codepointPos + 1);
        return edit->cursorBytePosition;
}

FILEPOS get_position_prev_codepoint(struct TextEdit *edit)
{
        FILEPOS codepointPos = compute_codepoint_position(edit->rope, edit->cursorBytePosition);
        if (codepointPos > 0)
                return compute_pos_of_codepoint(edit->rope, codepointPos - 1);
        return edit->cursorBytePosition;
}

FILEPOS get_position_next_word(struct TextEdit *edit)
{
        FILEPOS stopPos = edit->cursorBytePosition;
        FILEPOS linePos = get_position_line_begin(edit);
        struct Blunt_ReadCtx readCtx;
        begin_lexing_blunt_tokens(&readCtx, edit->rope, linePos);
        for (;;) {
                if (readCtx.readPos > stopPos)
                        break;
                struct Blunt_Token token;
                lex_blunt_token(&readCtx, &token);
                if (token.tokenKind == BLUNT_TOKEN_EOF)
                        break;
                find_start_of_next_token(&readCtx);
        }
        return readCtx.readPos;
}

FILEPOS get_position_previous_word(struct TextEdit *edit)
{
        FILEPOS previousLinePos;
        {
                /* we need to start in the line before the current line, in case the
                previous word is on the previous line */
                FILEPOS lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
                FILEPOS previousLineNumber = lineNumber - 1;
                if (previousLineNumber < 0)
                        previousLineNumber = 0;
                previousLinePos = compute_pos_of_line(edit->rope, previousLineNumber);
        }
        FILEPOS stopPos = edit->cursorBytePosition;
        struct Blunt_ReadCtx readCtx;
        begin_lexing_blunt_tokens(&readCtx, edit->rope, previousLinePos);
        FILEPOS lastPos = 0;
        for (;;) {
                if (readCtx.readPos >= stopPos)
                        break;
                lastPos = readCtx.readPos;
                struct Blunt_Token token;
                lex_blunt_token(&readCtx, &token);
                if (token.tokenKind == BLUNT_TOKEN_EOF)
                        break;
                find_start_of_next_token(&readCtx);
        }
        return lastPos;
}

FILEPOS get_position_codepoints_relative(struct TextEdit *edit, FILEPOS codepointsDiff)
{
        FILEPOS totalCodepoints = textrope_number_of_codepoints(edit->rope);
        FILEPOS oldCodepointPos = compute_codepoint_position(edit->rope, edit->cursorBytePosition);
        FILEPOS codepointPos = oldCodepointPos + codepointsDiff;
        if (codepointPos < 0)
                codepointPos = 0;
        else if (codepointPos > totalCodepoints)
                codepointPos = totalCodepoints;
        return compute_pos_of_codepoint(edit->rope, codepointPos);
}

FILEPOS get_position_of_line_and_column(struct TextEdit *edit, FILEPOS lineNumber, FILEPOS codepointColumn)
{
        FILEPOS linePos = compute_pos_of_line(edit->rope, lineNumber);
        FILEPOS lineCodepointPosition = compute_codepoint_position(edit->rope, linePos);
        FILEPOS nextLinePos = compute_pos_of_line(edit->rope, lineNumber + 1);
        FILEPOS nextLineCodepointPosition = compute_codepoint_position(edit->rope, nextLinePos);
        FILEPOS codepointsInLine = nextLineCodepointPosition - lineCodepointPosition;
        if (codepointColumn >= codepointsInLine) {
                if (codepointsInLine > 0) {  // this doesn't hold in a quirky (last) line
                        codepointColumn = codepointsInLine - 1;
                        if (codepointColumn > 0)  /* don't place on the last column (newline) but on the column before that. Good idea? */
                                codepointColumn--;
                }
        }
        FILEPOS codepointPos = lineCodepointPosition + codepointColumn;
        return compute_pos_of_codepoint(edit->rope, codepointPos);
}

FILEPOS get_position_lines_relative(struct TextEdit *edit, FILEPOS linesDiff)
{
        FILEPOS oldLineNumber;
        FILEPOS oldCodepointPosition;
        compute_line_number_and_codepoint_position(edit->rope, edit->cursorBytePosition,
                &oldLineNumber, &oldCodepointPosition);
        FILEPOS newLineNumber = oldLineNumber + linesDiff;
        if (newLineNumber < 0)
                newLineNumber = 0;
        else if (newLineNumber >= textrope_number_of_lines(edit->rope))
                newLineNumber = textrope_number_of_lines(edit->rope);
        FILEPOS oldLinePos = compute_pos_of_line(edit->rope, oldLineNumber);
        FILEPOS oldLineCodepointPosition = compute_codepoint_position(edit->rope, oldLinePos);
        FILEPOS codepointColumn = oldCodepointPosition - oldLineCodepointPosition;
        return get_position_of_line_and_column(edit, newLineNumber, codepointColumn);
}

FILEPOS get_position_line_begin(struct TextEdit *edit)
{
        FILEPOS lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
        return compute_pos_of_line(edit->rope, lineNumber);
}

FILEPOS get_position_line_end(struct TextEdit *edit)
{
        FILEPOS textropeLength = textrope_length(edit->rope);
        FILEPOS lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
        if (lineNumber == textrope_number_of_lines_quirky(edit->rope) - 1)
                if (lineNumber != textrope_number_of_lines(edit->rope) - 1)
                        return textropeLength;  // special handling for quirky lines
        FILEPOS nextLinePos = compute_pos_of_line(edit->rope, lineNumber + 1);
        FILEPOS nextLineCodepointPos = compute_codepoint_position(edit->rope, nextLinePos);
        FILEPOS codepointPos = nextLineCodepointPos - 1;  // should be '\n'
        return compute_pos_of_codepoint(edit->rope, codepointPos);  // assuming only a single '\n' at line end
}

FILEPOS get_position_left(struct TextEdit *edit)
{
        FILEPOS codepointPos = compute_codepoint_position(edit->rope, edit->cursorBytePosition);
        FILEPOS linebeginPos = get_position_line_begin(edit);
        if (edit->cursorBytePosition > linebeginPos)
                return compute_pos_of_codepoint(edit->rope, codepointPos - 1);
        return edit->cursorBytePosition;
}

FILEPOS get_position_right(struct TextEdit *edit)
{
        FILEPOS nextCodepointPos = get_position_next_codepoint(edit);
        FILEPOS lineendPos = get_position_line_end(edit);
        if (nextCodepointPos < lineendPos)
                return nextCodepointPos;
        return edit->cursorBytePosition;
}

FILEPOS get_position_up(struct TextEdit *edit)
{
        return get_position_lines_relative(edit, -1);
}

FILEPOS get_position_down(struct TextEdit *edit)
{
        return get_position_lines_relative(edit, 1);
}

FILEPOS get_position_pageup(struct TextEdit *edit)
{
        return get_position_lines_relative(edit, -LINES_PER_PAGE);
}

FILEPOS get_position_pagedown(struct TextEdit *edit)
{
        return get_position_lines_relative(edit, LINES_PER_PAGE);
}

FILEPOS get_position_first_line(struct TextEdit *edit)
{
        return compute_pos_of_line(edit->rope, 0);  // that should be zero I guess?
}

FILEPOS get_position_last_line(struct TextEdit *edit)
{
        FILEPOS numberOfLines = textrope_number_of_lines(edit->rope);
        return compute_pos_of_line(edit->rope, numberOfLines);
}

FILEPOS get_position_of_line(struct TextEdit *edit, FILEPOS lineNumber)
{
        return get_position_of_line_and_column(edit, lineNumber, 0);
}

FILEPOS get_movement_position(struct TextEdit *edit, struct Movement *movement)
{
        switch (movement->movementKind) {
        case MOVEMENT_LEFT:   return get_position_left(edit);
        case MOVEMENT_RIGHT:  return get_position_right(edit);
        case MOVEMENT_UP:     return get_position_up(edit);
        case MOVEMENT_DOWN:   return get_position_down(edit);
        case MOVEMENT_NEXT_CODEPOINT: return get_position_next_codepoint(edit);
        case MOVEMENT_PREVIOUS_CODEPOINT: return get_position_prev_codepoint(edit);
        case MOVEMENT_NEXT_WORD: return get_position_next_word(edit);
        case MOVEMENT_PREVIOUS_WORD: return get_position_previous_word(edit);
        case MOVEMENT_PAGEUP: return get_position_pageup(edit);
        case MOVEMENT_PAGEDOWN: return get_position_pagedown(edit);
        case MOVEMENT_LINEBEGIN: return get_position_line_begin(edit);
        case MOVEMENT_LINEEND:   return get_position_line_end(edit);
        case MOVEMENT_FIRSTLINE: return get_position_first_line(edit);
        case MOVEMENT_LASTLINE:  return get_position_last_line(edit);
        case MOVEMENT_SPECIFICLINE: return get_position_of_line(edit, movement->pos1);
        case MOVEMENT_SPECIFICLINEANDCOLUMN: return get_position_of_line_and_column(edit, movement->pos1, movement->pos2);
        default: fatal("Not implemented or invalid value!\n");
        }
}

void move_cursor_with_movement(struct TextEdit *edit, struct Movement *movement, int isSelecting)
{
        FILEPOS bytePos = get_movement_position(edit, movement);
        move_cursor_to_byte_position(edit, bytePos, isSelecting);
}

void delete_with_movement(struct TextEdit *edit, struct Movement *movement)
{
        FILEPOS startPos = edit->cursorBytePosition;
        FILEPOS endPos = get_movement_position(edit, movement);
        make_range(startPos, endPos, &startPos, &endPos);
        ENSURE(startPos <= endPos);
        erase_text_from_textedit(edit, startPos, endPos - startPos);
        move_cursor_to_byte_position(edit, startPos, 0);
}

void move_cursor_lines_relative(struct TextEdit *edit, FILEPOS linesDiff, int isSelecting)
{
        FILEPOS pos = get_position_lines_relative(edit, linesDiff);
        move_cursor_to_byte_position(edit, pos, isSelecting);
}

void delete_current_line(struct TextEdit *edit)
{
        move_cursor_to_beginning_of_line(edit, 0);
        FILEPOS startpos = edit->cursorBytePosition;
        FILEPOS endpos = get_position_line_end(edit);
        if (endpos != textrope_length(edit->rope)) {
                FILEPOS codepointPos = compute_codepoint_position(edit->rope, endpos);
                endpos = compute_pos_of_codepoint(edit->rope, codepointPos + 1);
        }
        erase_text_from_textedit(edit, startpos, endpos - startpos);
}

void scroll_up_one_page(struct TextEdit *edit, int isSelecting)
{
        move_cursor_lines_relative(edit, -LINES_PER_PAGE, isSelecting);
}

void scroll_down_one_page(struct TextEdit *edit, int isSelecting)
{
        move_cursor_lines_relative(edit, LINES_PER_PAGE, isSelecting);
}

//XXX move somewhere else
void insert_codepoints_into_textedit(struct TextEdit *edit, FILEPOS insertPos, uint32_t *codepoints, int numCodepoints)
{
        int codepointsPos = 0;
        FILEPOS ropePos = insertPos;
        while (codepointsPos < numCodepoints) {
                char buf[512];
                int bufFill;
                encode_utf8_span(codepoints, codepointsPos, numCodepoints, buf, sizeof buf, &codepointsPos, &bufFill);
                insert_text_into_textedit(edit, ropePos, &buf[0], bufFill, ropePos);
                ropePos += bufFill;
        }
}

void insert_codepoint_into_textedit(struct TextEdit *edit, uint32_t codepoint)
{
        insert_codepoints_into_textedit(edit, edit->cursorBytePosition, &codepoint, 1);
}

void erase_selected_in_TextEdit(struct TextEdit *edit)
{
        ENSURE(edit->isSelectionMode);
        FILEPOS start;
        FILEPOS onePastEnd;
        get_selected_range_in_bytes(edit, &start, &onePastEnd);
        erase_text_from_textedit(edit, start, onePastEnd - start);
        edit->isSelectionMode = 0;
        edit->cursorBytePosition = start;
}

void erase_forwards_in_TextEdit(struct TextEdit *edit)
{
        FILEPOS codepointPosition = compute_codepoint_position(edit->rope, edit->cursorBytePosition);
        FILEPOS totalCodepoints = textrope_number_of_codepoints(edit->rope);
        if (codepointPosition < totalCodepoints) {
                FILEPOS start = edit->cursorBytePosition;
                FILEPOS end = compute_pos_of_codepoint(edit->rope, codepointPosition + 1);
                if (start < end)
                        erase_text_from_textedit(edit, start, end - start);
        }
}

void erase_backwards_in_TextEdit(struct TextEdit *edit)
{
        FILEPOS end = edit->cursorBytePosition;
        move_cursor_left(edit, 0); //XXX
        FILEPOS start = edit->cursorBytePosition;
        if (start < end)
                erase_text_from_textedit(edit, start, end - start);
}

void send_notification_to_textedit(struct TextEdit *edit, int notificationKind, const char *notification, int notificationLength)
{
        /* What to do when there is already a message? */
        edit->haveNotification = 1;
        if (notificationLength > LENGTH(edit->notificationBuffer) - 1)
                notificationLength = LENGTH(edit->notificationBuffer) - 1;
        edit->haveNotification = 1;
        edit->notificationKind = notificationKind;
        edit->notificationLength = notificationLength;
        copy_string_and_zeroterminate(edit->notificationBuffer, notification, notificationLength);
}

void init_TextEdit(struct TextEdit *edit)
{
        UNUSED(edit);
        edit->rope = create_textrope();
        setup_vistate(&edit->vistate);
        setup_LinescrollAnimation(&edit->scrollAnimation);

        edit->cursorBytePosition = 0;
        edit->firstLineDisplayed = 0;
        edit->numberOfLinesDisplayed = 15;  // XXX need some mechanism to set and update this

        edit->isVimodeActive = 0;

        edit->isSelectionMode = 0;
        edit->selectionStartBytePosition = 0;

        edit->loading.isActive = 0;
        edit->saving.isActive = 0;

        edit->haveNotification = 0;
        edit->notificationLength = 0;
        edit->notificationBuffer[0] = 0;
}

void exit_TextEdit(struct TextEdit *edit)
{
        destroy_textrope(edit->rope);
        teardown_LinescrollAnimation(&edit->scrollAnimation);
        teardown_vistate(&edit->vistate);

        if (edit->loading.isActive) {
                cancel_thread_and_wait(edit->loading.threadHandle);
                dispose_thread(edit->loading.threadHandle);
        }
        if (edit->saving.isActive) {
                cancel_thread_and_wait(edit->saving.threadHandle);
                dispose_thread(edit->saving.threadHandle);
        }
}

/* TODO: maybe introduce TIMETICK event or sth like that? */
void update_textedit(struct TextEdit *edit)
{
        if (edit->scrollAnimation.isActive)
                update_LinescrollAnimation(&edit->scrollAnimation);
        check_if_loading_completed_and_if_so_then_cleanup(&edit->loading);
        check_if_saving_completed_and_if_so_then_cleanup(&edit->saving);
}
