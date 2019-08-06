#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/utf8.h>
#include <astedit/logging.h>
#include <astedit/textrope.h>
#include <astedit/textedit.h>
#include <string.h>  // XXX memcpy()


int textedit_length_in_bytes(struct TextEdit *edit)
{
        UNUSED(edit);
        return textrope_length(edit->rope);
}

int read_from_textedit(struct TextEdit *edit, int offset, char *dstBuffer, int size)
{
        UNUSED(edit);
        return copy_text_from_textrope(edit->rope, offset, dstBuffer, size);
}

void erase_from_textedit(struct TextEdit *edit, int offset, int length)
{
        UNUSED(edit);
        erase_text_from_textrope(edit->rope, offset, length);
}

int read_character_from_textedit(struct TextEdit *edit, int pos)
{
        char c;
        int numBytes = read_from_textedit(edit, pos, &c, 1);
        ENSURE(numBytes == 1);
        return c;
}


static void move_minimally_to_display_line(struct TextEdit *edit, int lineNumber)
{
        if (edit->firstLineDisplayed > lineNumber)
                edit->firstLineDisplayed = lineNumber;
        if (edit->firstLineDisplayed < lineNumber - edit->numberOfLinesDisplayed + 1)
                edit->firstLineDisplayed = lineNumber - edit->numberOfLinesDisplayed + 1;
}

static void move_minimally_to_display_cursor(struct TextEdit *edit)
{
        int lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
        move_minimally_to_display_line(edit, lineNumber);
}


static void move_cursor_to_codepoint(struct TextEdit *edit, int codepointPos)
{
        int pos = compute_pos_of_codepoint(edit->rope, codepointPos);
        edit->cursorBytePosition = pos;
        move_minimally_to_display_cursor(edit);
}

static void move_cursor_left(struct TextEdit *edit)
{
        int codepointPosition = compute_codepoint_position(edit->rope, edit->cursorBytePosition);
        //int totalCodepoints = textrope_number_of_codepoints(edit->rope);
        if (codepointPosition > 0) {
                int pos = compute_pos_of_codepoint(edit->rope, codepointPosition - 1);
                edit->cursorBytePosition = pos;
                move_minimally_to_display_cursor(edit);
                log_postf("Cursor is in line %d", compute_line_number(edit->rope, pos));
        }
}

static void move_cursor_right(struct TextEdit *edit)
{
        int codepointPosition = compute_codepoint_position(edit->rope, edit->cursorBytePosition);
        int totalCodepoints = textrope_number_of_codepoints(edit->rope);
        if (codepointPosition < totalCodepoints) {  // we may move one past end
                int pos = compute_pos_of_codepoint(edit->rope, codepointPosition + 1);
                edit->cursorBytePosition = pos;
                move_minimally_to_display_cursor(edit);
                log_postf("Cursor is in line %d", compute_line_number(edit->rope, pos));
        }
}



static void move_to_line_and_column(struct TextEdit *edit, int lineNumber, int codepointColumn)
{
        int linePos = compute_pos_of_line(edit->rope, lineNumber);
        int lineCodepointPosition = compute_codepoint_position(edit->rope, linePos);
        int nextLinePos = compute_pos_of_line(edit->rope, lineNumber + 1);
        int nextLineCodepointPosition = compute_codepoint_position(edit->rope, nextLinePos);

        int codepointsInLine = nextLineCodepointPosition - lineCodepointPosition;
        ENSURE(codepointsInLine > 0);  // at least '\n'
        if (codepointColumn >= codepointsInLine - 1) {
                codepointColumn = codepointsInLine - 1;
                if (codepointColumn > 0)  /* don't place on the last column (newline) but on the column before that. Good idea? */
                        codepointColumn--;
        }

        int newPos = compute_pos_of_codepoint(edit->rope, lineCodepointPosition + codepointColumn);
        edit->cursorBytePosition = newPos;

        move_minimally_to_display_line(edit, lineNumber);
}


static void move_lines_relative(struct TextEdit *edit, int linesDiff)
{
        int oldLineNumber;
        int oldCodepointPosition;
        compute_line_number_and_codepoint_position(edit->rope, edit->cursorBytePosition,
                &oldLineNumber, &oldCodepointPosition);

        int newLineNumber = oldLineNumber + linesDiff;
        if (0 <= newLineNumber && newLineNumber < textrope_number_of_lines_quirky(edit->rope)) {
                int oldLinePos = compute_pos_of_line(edit->rope, oldLineNumber);
                int oldLineCodepointPosition = compute_codepoint_position(edit->rope, oldLinePos);
                int codepointColumn = oldCodepointPosition - oldLineCodepointPosition;
                move_to_line_and_column(edit, newLineNumber, codepointColumn);
        }
}

static void move_cursor_up(struct TextEdit *edit)
{
        move_lines_relative(edit, -1);
}

static void move_cursor_down(struct TextEdit *edit)
{
        move_lines_relative(edit, +1);
}

static void move_cursor_to_beginning_of_line(struct TextEdit *edit)
{
        int lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
        int pos = compute_pos_of_line(edit->rope, lineNumber);
        edit->cursorBytePosition = pos;
        move_minimally_to_display_cursor(edit);
}

static void move_cursor_to_end_of_line(struct TextEdit *edit)
{
        int numberOfLines = textrope_number_of_lines_quirky(edit->rope);
        int lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
        if (lineNumber >= numberOfLines)
                return;  // i think this happens when the cursor is editing at the ending position of the text
        int pos = compute_pos_of_line(edit->rope, lineNumber + 1);
        int codepointPos = compute_codepoint_position(edit->rope, pos);
        ENSURE(codepointPos > 0);
        move_cursor_to_codepoint(edit, codepointPos - 1);
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
        move_cursor_right(edit);
}

static void erase_forwards(struct TextEdit *edit)
{
        int codepointPosition = compute_codepoint_position(edit->rope, edit->cursorBytePosition);
        int totalCodepoints = textrope_number_of_codepoints(edit->rope);
        if (codepointPosition < totalCodepoints) {
                int start = edit->cursorBytePosition;
                int end = compute_pos_of_codepoint(edit->rope, codepointPosition + 1);
                if (start < end)
                        erase_from_textedit(edit, start, end - start);
        }
}

static void erase_backwards(struct TextEdit *edit)
{
        int end = edit->cursorBytePosition;
        move_cursor_left(edit);
        int start = edit->cursorBytePosition;
        if (start < end)
                erase_from_textedit(edit, start, end - start);
}

void process_input_in_textEdit(struct Input *input, struct TextEdit *edit)
{
        if (input->inputKind == INPUT_KEY) {
                switch (input->data.tKey.keyKind) {
                case KEY_ENTER:
                        insert_codepoint_into_textedit(edit, 0x0a);
                        break;
                case KEY_CURSORLEFT:
                        move_cursor_left(edit);
                        break;
                case KEY_CURSORRIGHT:
                        move_cursor_right(edit);
                        break;
                case KEY_CURSORUP:
                        move_cursor_up(edit);
                        break;
                case KEY_CURSORDOWN:
                        move_cursor_down(edit);
                        break;
                case KEY_HOME:
                        move_cursor_to_beginning_of_line(edit);
                        break;
                case KEY_END:
                        move_cursor_to_end_of_line(edit);
                        break;
                case KEY_DELETE:
                        erase_forwards(edit);
                        break;
                case KEY_BACKSPACE:
                        erase_backwards(edit);
                        break;
                default:
                        if (input->data.tKey.hasCodepoint) {
                                unsigned long codepoint = input->data.tKey.codepoint;
                                insert_codepoint_into_textedit(edit, codepoint);
                                debug_check_textrope(edit->rope);
                        }
                        break;
                }


        }
}

void init_TextEdit(struct TextEdit *edit)
{
        UNUSED(edit);
        edit->rope = create_textrope();
        edit->cursorBytePosition = 0;
        edit->firstLineDisplayed = 0;
        edit->numberOfLinesDisplayed = 15;  // XXX need some mechanism to set and update this
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
        }
        if (ferror(f))
                fatalf("Errors while reading from file %s\n", filepath);
        if (bufFill > 0)
                fatalf("Unconsumed characters at the end!\n");
        fclose(f);

        edit->cursorBytePosition = 0;

        print_textrope_statistics(edit->rope);
}
