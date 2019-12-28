#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/utf8.h>
#include <astedit/logging.h>
#include <astedit/osthread.h>
#include <astedit/textrope.h>
#include <astedit/textedit.h>
#include <astedit/texteditloadsave.h>
#include <astedit/texteditsearch.h>
#include <astedit/edithistory.h>
#include <astedit/sound.h>
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

static void get_position_codepoint(struct TextEdit *edit, struct FileCursor *cursor, FILEPOS codepointPos)
{
        FILEPOS totalCodepoints = textrope_number_of_codepoints(edit->rope);
        if (codepointPos < 0) {
                cursor->didHitBoundary = 1;
                cursor->bytePosition = 0;
        }
        else if (codepointPos > totalCodepoints) {
                cursor->didHitBoundary = 1;
                cursor->bytePosition = textrope_length(edit->rope);
        }
        else {
                cursor->bytePosition = compute_pos_of_codepoint(edit->rope, codepointPos);
        }
}

void get_position_of_line(struct TextEdit *edit, struct FileCursor *fc, FILEPOS lineNumber)
{
        FILEPOS totalLines = textrope_number_of_lines_quirky(edit->rope);
        if (lineNumber < 0) {
                fc->didHitBoundary = 1;
                fc->bytePosition = 0;
        }
        else if (lineNumber >= totalLines) {
                fc->didHitBoundary = 1;
                fc->bytePosition = textrope_length(edit->rope);
        }
        else {
                fc->bytePosition = compute_pos_of_line(edit->rope, lineNumber);
        }
}

void get_position_next_codepoint(struct TextEdit *edit, struct FileCursor *fc)
{
        FILEPOS codepointPos = compute_codepoint_position(edit->rope, fc->bytePosition);
        get_position_codepoint(edit, fc, codepointPos + 1);
}

void get_position_prev_codepoint(struct TextEdit *edit, struct FileCursor *fc)
{
        FILEPOS codepointPos = compute_codepoint_position(edit->rope, fc->bytePosition);
        get_position_codepoint(edit, fc, codepointPos - 1);
}

void get_position_next_word(struct TextEdit *edit, struct FileCursor *fc)
{
        FILEPOS stopPos = fc->bytePosition;
        FILEPOS linePos = compute_line_number(edit->rope, fc->bytePosition);
        struct Blunt_ReadCtx readCtx;
        begin_lexing_blunt_tokens(&readCtx, edit->rope, linePos);
        for (;;) {
                if (readCtx.readPos > stopPos)
                        break;
                struct Blunt_Token token;
                lex_blunt_token(&readCtx, &token);
                if (token.tokenKind == BLUNT_TOKEN_EOF) {
                        fc->didHitBoundary = 1;
                        break;
                }
                find_start_of_next_token(&readCtx);
        }
        fc->bytePosition = readCtx.readPos;
}

void get_position_previous_word(struct TextEdit *edit, struct FileCursor *fc)
{
        struct Blunt_ReadCtx readCtx;
        FILEPOS lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
        for (;;) {
                FILEPOS pos = compute_pos_of_line(edit->rope, lineNumber);
                begin_lexing_blunt_tokens(&readCtx, edit->rope, pos);
                find_start_of_next_token(&readCtx);
                if (readCtx.readPos < edit->cursorBytePosition)
                        break;
                if (lineNumber == 0) {
                        fc->bytePosition = 0;
                        fc->didHitBoundary = 1;
                        return;
                }
                lineNumber --;
        }
        FILEPOS stopPos = edit->cursorBytePosition;
        FILEPOS lastPos;
        for (;;) {
                lastPos = readCtx.readPos;
                struct Blunt_Token token;
                lex_blunt_token(&readCtx, &token);
                ENSURE(token.tokenKind != BLUNT_TOKEN_EOF); // above we found the start of this token to be before the previous cursorBytePosition
                find_start_of_next_token(&readCtx);
                if (readCtx.readPos >= stopPos)
                        break;
        }
        fc->bytePosition = lastPos;
}

void get_position_codepoints_relative(struct TextEdit *edit, struct FileCursor *fc, FILEPOS codepointsDiff)
{
        FILEPOS oldCodepointPos = compute_codepoint_position(edit->rope, fc->bytePosition);
        get_position_codepoint(edit, fc, oldCodepointPos + codepointsDiff);
}

void get_position_lines_relative(struct TextEdit *edit, struct FileCursor *fc, FILEPOS linesDiff)
{
        FILEPOS oldLineNumber;
        FILEPOS oldCodepointPosition;
        compute_line_number_and_codepoint_position(edit->rope, edit->cursorBytePosition,
                &oldLineNumber, &oldCodepointPosition);
        FILEPOS newLineNumber = oldLineNumber + linesDiff;
        if (newLineNumber < 0) {
                fc->didHitBoundary = 1;
                newLineNumber = 0;
        }
        else if (newLineNumber > textrope_number_of_lines(edit->rope)) {
                fc->didHitBoundary = 1;
                newLineNumber = textrope_number_of_lines(edit->rope);
        }
        FILEPOS oldLinePos = compute_pos_of_line(edit->rope, oldLineNumber);
        FILEPOS oldLineCodepointPosition = compute_codepoint_position(edit->rope, oldLinePos);
        FILEPOS codepointColumn = oldCodepointPosition - oldLineCodepointPosition;
        get_position_of_line_and_column(edit, fc, newLineNumber, codepointColumn);
}

void get_position_of_line_and_column(struct TextEdit *edit, struct FileCursor *fc, FILEPOS lineNumber, FILEPOS codepointColumn)
{
        ENSURE(codepointColumn >= 0);
        get_position_of_line(edit, fc, lineNumber);  // we use that because it might set didHitBoundary
        // is this right?
        get_position_codepoints_relative(edit, fc, codepointColumn);
}

void get_position_line_begin(struct TextEdit *edit, struct FileCursor *fc)
{
        FILEPOS lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
        get_position_of_line_and_column(edit, fc, lineNumber, 0);
}

void get_position_line_end(struct TextEdit *edit, struct FileCursor *fc)
{
        FILEPOS lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
        if (lineNumber == textrope_number_of_lines_quirky(edit->rope) - 1) {
                if (lineNumber != textrope_number_of_lines(edit->rope) - 1) {
                        fc->bytePosition = textrope_length(edit->rope);
                        return;
                }
        }
        FILEPOS nextLinePos = compute_pos_of_line(edit->rope, lineNumber + 1);
        FILEPOS nextLineCodepointPos = compute_codepoint_position(edit->rope, nextLinePos);
        FILEPOS codepointPos = nextLineCodepointPos - 1;  // should be '\n'
        get_position_codepoint(edit, fc, codepointPos);
}

void get_position_left(struct TextEdit *edit, struct FileCursor *fc)
{
        FILEPOS lineNumber = compute_line_number(edit->rope, fc->bytePosition);
        FILEPOS linebeginPos = compute_pos_of_line(edit->rope, lineNumber);
        if (edit->cursorBytePosition == linebeginPos)
                fc->didHitBoundary = 1;
        else
                get_position_prev_codepoint(edit, fc);
}

void get_position_right(struct TextEdit *edit, struct FileCursor *fc)
{
        //XXX
        struct FileCursor lineEnd = { fc->bytePosition, 0 };
        get_position_line_end(edit, &lineEnd);
        if (edit->cursorBytePosition == lineEnd.bytePosition)  // TODO: lineendPos is where the newline char is, right?
                fc->didHitBoundary = 1;
        else
                get_position_next_codepoint(edit, fc);
}



/* these are just little wrappers to translate keyboard actions. Should we use a
 * different naming scheme? */

void get_position_up(struct TextEdit *edit, struct FileCursor *fc)
{
        get_position_lines_relative(edit, fc, -1);
}

void get_position_down(struct TextEdit *edit, struct FileCursor *fc)
{
        get_position_lines_relative(edit, fc, 1);
}

void get_position_pageup(struct TextEdit *edit, struct FileCursor *fc)
{
        get_position_lines_relative(edit, fc, -LINES_PER_PAGE);
}

void get_position_pagedown(struct TextEdit *edit, struct FileCursor *fc)
{
        get_position_lines_relative(edit, fc, LINES_PER_PAGE);
}

void get_position_first_line(struct TextEdit *edit, struct FileCursor *fc)
{
        get_position_of_line(edit, fc, 0);
}

void get_position_last_line(struct TextEdit *edit, struct FileCursor *fc)
{
        FILEPOS numberOfLines = textrope_number_of_lines(edit->rope);
        // XXX: isn't that conditional a little strange?
        if (numberOfLines == 0)
                fc->bytePosition = 0;
        else
                get_position_of_line(edit, fc, numberOfLines - 1);
}

void get_position_next_match(struct TextEdit *edit, struct FileCursor *fc)
{
        FILEPOS matchStart;
        FILEPOS matchEnd;
        if (search_next_match(edit, &matchStart, &matchEnd))
                fc->bytePosition = matchStart;
        else
                fc->didHitBoundary = 1; //XXX ???
}

FILEPOS get_movement_position(struct TextEdit *edit, struct Movement *movement)
{
        // for now.
        struct FileCursor fc = { edit->cursorBytePosition, 0 };
        switch (movement->movementKind) {
        case MOVEMENT_LEFT:   get_position_left(edit, &fc); break;
        case MOVEMENT_RIGHT:  get_position_right(edit, &fc); break;
        case MOVEMENT_UP:     get_position_up(edit, &fc); break;
        case MOVEMENT_DOWN:   get_position_down(edit, &fc); break;
        case MOVEMENT_NEXT_CODEPOINT: get_position_next_codepoint(edit, &fc); break;
        case MOVEMENT_PREVIOUS_CODEPOINT: get_position_prev_codepoint(edit, &fc); break;
        case MOVEMENT_NEXT_WORD: get_position_next_word(edit, &fc); break;
        case MOVEMENT_PREVIOUS_WORD: get_position_previous_word(edit, &fc); break;
        case MOVEMENT_PAGEUP: get_position_pageup(edit, &fc); break;
        case MOVEMENT_PAGEDOWN: get_position_pagedown(edit, &fc); break;
        case MOVEMENT_LINEBEGIN: get_position_line_begin(edit, &fc); break;
        case MOVEMENT_LINEEND:   get_position_line_end(edit, &fc); break;
        case MOVEMENT_FIRSTLINE: get_position_first_line(edit, &fc); break;
        case MOVEMENT_LASTLINE:  get_position_last_line(edit, &fc); break;
        case MOVEMENT_SPECIFICLINE: get_position_of_line(edit, &fc, movement->pos1); break;
        case MOVEMENT_SPECIFICLINEANDCOLUMN: get_position_of_line_and_column(edit, &fc, movement->pos1, movement->pos2); break;
        case MOVEMENT_NEXT_MATCH: get_position_next_match(edit, &fc); break;
        default: fatal("Not implemented or invalid value!\n");
        }
        if (fc.didHitBoundary)
                play_navigation_impossible_sound();
        return fc.bytePosition;
}

static void move_view_minimally_to_display_cursor(struct TextEdit *edit)
{
        FILEPOS lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
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

static void move_cursor_to_byte_position(struct TextEdit *edit, FILEPOS pos, int isSelecting)
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

void delete_current_line(struct TextEdit *edit)
{
        FILEPOS lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
        FILEPOS startpos = compute_pos_of_line(edit->rope, lineNumber);
        FILEPOS endpos = compute_pos_of_line(edit->rope, lineNumber + 1);
        erase_text_from_textedit(edit, startpos, endpos - startpos);
        edit->cursorBytePosition = startpos;
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

#include <stdio.h>
void send_notification_to_textedit_f(struct TextEdit *edit, int notificationKind, const char *fmt, ...)
{
        char buf[512]; //XXX
        va_list ap;
        va_start(ap, fmt);
        int r = vsnprintf(buf, sizeof buf, fmt, ap);
        va_end(ap);

        if (r < 0 || r > sizeof buf - 1)
                return;
        send_notification_to_textedit(edit, notificationKind, buf, r);
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
