#include <astedit/astedit.h>
#include <astedit/filepositions.h>
#include <astedit/textedit.h>
#include <astedit/textpositions.h>
#include <astedit/texteditsearch.h>
#include <blunt/lex.h> //XXX



enum {
        LINES_PER_PAGE = 15,   // XXX this value should be dependent on the current GUI viewport probably.
};

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
        FILEPOS stopPos = fc->bytePosition;
        FILEPOS lineNumber = compute_line_number(edit->rope, stopPos);
        for (;;) {
                FILEPOS pos = compute_pos_of_line(edit->rope, lineNumber);
                begin_lexing_blunt_tokens(&readCtx, edit->rope, pos);
                find_start_of_next_token(&readCtx);
                if (readCtx.readPos < stopPos)
                        break;
                if (lineNumber == 0) {
                        fc->bytePosition = 0;
                        fc->didHitBoundary = 1;
                        return;
                }
                lineNumber --;
        }
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
        struct FileCursor fc2 = { fc->bytePosition, 0 };
        get_position_of_line(edit, &fc2, lineNumber + 1);
        get_position_of_line(edit, fc, lineNumber);  // we use that because it might set didHitBoundary
        get_position_codepoints_relative(edit, fc, codepointColumn);
        if (fc->bytePosition >= fc2.bytePosition)
                fc->bytePosition = fc2.bytePosition - 1;
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
        if (search_next_match(edit, &matchStart, &matchEnd)) {
                fc->bytePosition = matchStart;
        }
        else
                fc->didHitBoundary = 1; //XXX ???
}
