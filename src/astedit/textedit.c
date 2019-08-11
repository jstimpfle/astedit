#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/utf8.h>
#include <astedit/logging.h>
#include <astedit/textrope.h>
#include <astedit/textedit.h>


int textedit_length_in_bytes(struct TextEdit *edit)
{
        UNUSED(edit);
        return textrope_length(edit->rope);
}

static int read_character_from_textedit(struct TextEdit *edit, int pos)
{
        char c;
        int numBytes = read_from_textrope(edit->rope, pos, &c, 1);
        ENSURE(numBytes == 1);
        return c;
}



void get_selected_range_in_bytes(struct TextEdit *edit, int *outStart, int *outOnePastEnd)
{
        ENSURE(edit->isSelectionMode);
        if (edit->cursorBytePosition < edit->selectionStartBytePosition) {
                *outStart = edit->cursorBytePosition;
                *outOnePastEnd = edit->selectionStartBytePosition;
        }
        else {
                *outStart = edit->selectionStartBytePosition;
                *outOnePastEnd = edit->cursorBytePosition;
        };
}

void get_selected_range_in_codepoints(struct TextEdit *edit, int *outStart, int *outOnePastEnd)
{
        int start;
        int onePastEnd;
        get_selected_range_in_bytes(edit, &start, &onePastEnd);
        *outStart = compute_codepoint_position(edit->rope, start);
        *outOnePastEnd = compute_codepoint_position(edit->rope, onePastEnd);
}



void move_view_minimally_to_display_line(struct TextEdit *edit, int lineNumber)
{
        if (edit->firstLineDisplayed > lineNumber)
                edit->firstLineDisplayed = lineNumber;
        if (edit->firstLineDisplayed < lineNumber - edit->numberOfLinesDisplayed + 1)
                edit->firstLineDisplayed = lineNumber - edit->numberOfLinesDisplayed + 1;
}

void move_view_minimally_to_display_cursor(struct TextEdit *edit)
{
        int lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
        move_view_minimally_to_display_line(edit, lineNumber);
}

void move_cursor_to_byte_position(struct TextEdit *edit, int pos, int isSelecting)
{
        if (!edit->isSelectionMode && isSelecting)
                edit->selectionStartBytePosition = edit->cursorBytePosition;
        edit->isSelectionMode = isSelecting;

        edit->cursorBytePosition = pos;
        move_view_minimally_to_display_cursor(edit);
        //log_postf("Cursor is in line %d", compute_line_number(edit->rope, pos));
}

void move_cursor_to_codepoint(struct TextEdit *edit, int codepointPos, int isSelecting)
{
        int pos = compute_pos_of_codepoint(edit->rope, codepointPos);
        move_cursor_to_byte_position(edit, pos, isSelecting);
}

void move_cursor_left(struct TextEdit *edit, int isSelecting)
{
        int codepointPosition = compute_codepoint_position(edit->rope, edit->cursorBytePosition);
        //int totalCodepoints = textrope_number_of_codepoints(edit->rope);
        if (codepointPosition > 0)
                move_cursor_to_codepoint(edit, codepointPosition - 1, isSelecting);
}

void move_cursor_right(struct TextEdit *edit, int isSelecting)
{
        int codepointPosition = compute_codepoint_position(edit->rope, edit->cursorBytePosition);
        int totalCodepoints = textrope_number_of_codepoints(edit->rope);
        if (codepointPosition < totalCodepoints)  // we may move one past end
                move_cursor_to_codepoint(edit, codepointPosition + 1, isSelecting);
}

void move_cursor_to_line_and_column(struct TextEdit *edit, int lineNumber, int codepointColumn, int isSelecting)
{
        int linePos = compute_pos_of_line(edit->rope, lineNumber);
        int lineCodepointPosition = compute_codepoint_position(edit->rope, linePos);
        int nextLinePos = compute_pos_of_line(edit->rope, lineNumber + 1);
        int nextLineCodepointPosition = compute_codepoint_position(edit->rope, nextLinePos);

        int codepointsInLine = nextLineCodepointPosition - lineCodepointPosition;
        if (codepointColumn > codepointsInLine) {
                codepointColumn = codepointsInLine;
                if (codepointColumn > 0)  /* don't place on the last column (newline) but on the column before that. Good idea? */
                        codepointColumn--;
        }

        move_cursor_to_codepoint(edit, lineCodepointPosition + codepointColumn, isSelecting);
}

void move_cursor_lines_relative(struct TextEdit *edit, int linesDiff, int isSelecting)
{
        int oldLineNumber;
        int oldCodepointPosition;
        compute_line_number_and_codepoint_position(edit->rope, edit->cursorBytePosition,
                &oldLineNumber, &oldCodepointPosition);

        int newLineNumber = oldLineNumber + linesDiff;

        if (newLineNumber < 0)
                newLineNumber = 0;
        else if (newLineNumber >= textrope_number_of_lines(edit->rope))
                newLineNumber = textrope_number_of_lines(edit->rope);

        int oldLinePos = compute_pos_of_line(edit->rope, oldLineNumber);
        int oldLineCodepointPosition = compute_codepoint_position(edit->rope, oldLinePos);
        int codepointColumn = oldCodepointPosition - oldLineCodepointPosition;
        move_cursor_to_line_and_column(edit, newLineNumber, codepointColumn, isSelecting);
}

void move_cursor_up(struct TextEdit *edit, int isSelecting)
{
        move_cursor_lines_relative(edit, -1, isSelecting);
}

void move_cursor_down(struct TextEdit *edit, int isSelecting)
{
        move_cursor_lines_relative(edit, +1, isSelecting);
}

void move_cursor_to_beginning_of_line(struct TextEdit *edit, int isSelecting)
{
        int lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
        int pos = compute_pos_of_line(edit->rope, lineNumber);
        move_cursor_to_byte_position(edit, pos, isSelecting);
}

void move_cursor_to_end_of_line(struct TextEdit *edit, int isSelecting)
{
        int numberOfLines = textrope_number_of_lines_quirky(edit->rope);
        int lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
        if (lineNumber >= numberOfLines)
                return;  // i think this happens when the cursor is editing at the ending position of the text
        int pos = compute_pos_of_line(edit->rope, lineNumber + 1);
        int codepointPos = compute_codepoint_position(edit->rope, pos);
        ENSURE(codepointPos > 0);
        move_cursor_to_codepoint(edit, codepointPos - 1, isSelecting);
}

void move_to_first_line(struct TextEdit *edit, int isSelecting)
{
        move_cursor_to_line_and_column(edit, 0, 0, isSelecting);
}

void move_to_last_line(struct TextEdit *edit, int isSelecting)
{
        move_cursor_to_line_and_column(edit,
                                textrope_number_of_lines(edit->rope),
                                0, isSelecting);
}

enum { LINES_PER_PAGE = 15 };

void scroll_up_one_page(struct TextEdit *edit, int isSelecting)
{
        move_cursor_lines_relative(edit, -LINES_PER_PAGE, isSelecting);
}

void scroll_down_one_page(struct TextEdit *edit, int isSelecting)
{
        move_cursor_lines_relative(edit, LINES_PER_PAGE, isSelecting);
}

void insert_codepoints_into_textedit(struct TextEdit *edit, int insertPos, uint32_t *codepoints, int numCodepoints)
{
        UNUSED(edit);
        char buf[512];
        int bufFill;
        int pos = 0;
        while (pos < numCodepoints) {
                encode_utf8_span(codepoints, pos, numCodepoints, buf, sizeof buf, &pos, &bufFill);
                insert_text_into_textrope(edit->rope, insertPos, &buf[0], bufFill);
                insertPos += bufFill;
        }
}

void insert_codepoint_into_textedit(struct TextEdit *edit, uint32_t codepoint)
{
        char tmp[16];
        int numBytes = encode_codepoint_as_utf8(codepoint, &tmp[0], 0, sizeof tmp);
        tmp[numBytes] = 0;  // for nicer debugging

        int insertPos = edit->cursorBytePosition;
        //int insertPos = textrope_length(textrope);

        insert_text_into_textrope(edit->rope, insertPos, &tmp[0], numBytes);
        move_cursor_right(edit, 0);  // XXX
}

void erase_selected_in_TextEdit(struct TextEdit *edit)
{
        ENSURE(edit->isSelectionMode);
        int start;
        int onePastEnd;
        get_selected_range_in_bytes(edit, &start, &onePastEnd);
        erase_text_from_textrope(edit->rope, start, onePastEnd - start);
        edit->isSelectionMode = 0;
        edit->cursorBytePosition = start;
}

void erase_forwards_in_TextEdit(struct TextEdit *edit)
{
        int codepointPosition = compute_codepoint_position(edit->rope, edit->cursorBytePosition);
        int totalCodepoints = textrope_number_of_codepoints(edit->rope);
        if (codepointPosition < totalCodepoints) {
                int start = edit->cursorBytePosition;
                int end = compute_pos_of_codepoint(edit->rope, codepointPosition + 1);
                if (start < end)
                        erase_text_from_textrope(edit->rope, start, end - start);
        }
}

void erase_backwards_in_TextEdit(struct TextEdit *edit)
{
        int end = edit->cursorBytePosition;
        move_cursor_left(edit, 0); //XXX
        int start = edit->cursorBytePosition;
        if (start < end)
                erase_text_from_textrope(edit->rope, start, end - start);
}

void init_TextEdit(struct TextEdit *edit)
{
        UNUSED(edit);
        edit->rope = create_textrope();

        edit->cursorBytePosition = 0;

        edit->isSelectionMode = 0;
        edit->selectionStartBytePosition = 0;

        edit->firstLineDisplayed = 0;
        edit->numberOfLinesDisplayed = 15;  // XXX need some mechanism to set and update this

        edit->isLoading = 0;
        edit->loadingCompletedBytes = 0;
        edit->loadingTotalBytes = 0;
}

void exit_TextEdit(struct TextEdit *edit)
{
        UNUSED(edit);
        destroy_textrope(edit->rope);
}

#include <stdio.h>
void textedit_test_init(struct TextEdit *edit, const char *filepath)
{
        FILE *f = fopen(filepath, "rb");
        if (!f)
                fatalf("Failed to open file %s\n", filepath);

        char buf[1024];
        int bufFill = 0;

        edit->isLoading = 1;
        edit->loadingCompletedBytes = 0;
        edit->loadingTotalBytes = 30000000;

        for (;;) {
                size_t n = fread(buf + bufFill, 1, sizeof buf - bufFill, f);
                bufFill += n;

                if (n == 0)
                        /* EOF. Ignore remaining undecoded bytes */
                        break;

                uint32_t utf8buf[LENGTH(buf)];
                int utf8Fill;

                decode_utf8_span_and_move_rest_to_front(buf, bufFill, utf8buf, &bufFill, &utf8Fill);
                insert_codepoints_into_textedit(edit, textrope_length(edit->rope), utf8buf, utf8Fill);

                edit->loadingCompletedBytes += utf8Fill;


                //XXXX need to be asynchronous
                extern void mainloop(void);
                mainloop();
        }
        if (ferror(f))
                fatalf("Errors while reading from file %s\n", filepath);
        if (bufFill > 0)
                fatalf("Unconsumed characters at the end!\n");

        fclose(f);

        edit->isLoading = 0;
        edit->cursorBytePosition = 0;

        print_textrope_statistics(edit->rope);
}
