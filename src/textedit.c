#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/utf8.h>
#include <astedit/logging.h>
#include <astedit/textrope.h>
#include <astedit/textedit.h>
#include <string.h>  // XXX memcpy()


static struct Textrope *textrope;


int textedit_length_in_bytes(struct TextEdit *edit)
{
        return textrope_length(textrope);
}

int read_from_textedit(struct TextEdit *edit, int offset, char *dstBuffer, int size)
{
        return copy_text_from_textrope(textrope, offset, dstBuffer, size);
}

void erase_from_textedit(struct TextEdit *edit, int offset, int length)
{
        erase_text_from_textrope(textrope, offset, length);
}

int read_character_from_textedit(struct TextEdit *edit, int pos)
{
        char c;
        int numBytes = read_from_textedit(edit, pos, &c, 1);
        ENSURE(numBytes == 1);
        return c;
}


static int find_previous_utf8_sequence(struct TextEdit *edit, int pos)
{
        while (pos > 0) {
                pos--;
                int c = read_character_from_textedit(edit, pos);
                if (is_utf8_leader_byte(c))
                        break;
        }
        return pos;
}

static int find_next_utf8_sequence(struct TextEdit *edit, int pos)
{
        int length = textedit_length_in_bytes(edit);
        while (pos < length) {
                pos++;
                if (pos == length)
                        break;
                int c = read_character_from_textedit(edit, pos);
                if (is_utf8_leader_byte(c))
                        break;
        }
        return pos;
}

static void move_cursor_left(struct TextEdit *edit)
{
        if (edit->cursorBytePosition > 0) {
                int offset = find_previous_utf8_sequence(edit, edit->cursorBytePosition);
                edit->cursorBytePosition = offset;
                edit->cursorCodepointPosition -= 1;
        }
}

static void move_cursor_right(struct TextEdit *edit)
{
        if (edit->cursorBytePosition < textedit_length_in_bytes(edit)) {
                int offset = find_next_utf8_sequence(edit, edit->cursorBytePosition);
                edit->cursorBytePosition = offset;
                edit->cursorCodepointPosition += 1;
        }
}

void insert_codepoint_into_textedit(struct TextEdit *edit, unsigned long codepoint)
{
        char tmp[16];
        int numBytes = encode_codepoint_as_utf8(codepoint, &tmp[0], 0, sizeof tmp);
        tmp[numBytes] = 0;  // for nicer debugging

        int insertPos = edit->cursorBytePosition;
        //int insertPos = textrope_length(textrope);

        insert_text_into_textrope(textrope, insertPos, &tmp[0], numBytes);
        move_cursor_right(edit);
}

static void erase_forwards(struct TextEdit *edit)
{
        int start = edit->cursorBytePosition;
        int end = find_next_utf8_sequence(edit, start);
        if (start < end)
                erase_from_textedit(edit, start, end - start);
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
                switch (input->tKey.keyKind) {
                case KEY_ENTER:
                        insert_codepoint_into_textedit(edit, 0x0a);
                        break;
                case KEY_CURSORLEFT:
                        move_cursor_left(edit);
                        break;
                case KEY_CURSORRIGHT:
                        move_cursor_right(edit);
                        break;
                case KEY_DELETE:
                        erase_forwards(edit);
                        break;
                case KEY_BACKSPACE:
                        erase_backwards(edit);
                        break;
                default:
                        if (input->tKey.hasCodepoint) {
                                unsigned long codepoint = input->tKey.codepoint;
                                insert_codepoint_into_textedit(edit, codepoint);
                        }
                        break;
                }


        }
}

void init_TextEdit(struct TextEdit *edit)
{
        textrope = create_textrope();
}

void exit_TextEdit(struct TextEdit *edit)
{
        destroy_textrope(textrope);
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
                if (n == 0)
                        break;
                bufFill += n;
                printf("read %d\n", (int) n);

                uint32_t utf8buf[1024];
                int utf8Fill;
                int decodeEnd;
                decode_utf8_span(buf, 0, bufFill, utf8buf, LENGTH(utf8buf), &decodeEnd, &utf8Fill);

                for (int i = 0; i < utf8Fill; i++)
                        insert_codepoint_into_textedit(edit, utf8buf[i]);

                move_memory(buf + decodeEnd, -decodeEnd, bufFill - decodeEnd);
                bufFill -= decodeEnd;
        }
        if (ferror(f))
                fatalf("Errors while reading from file %s\n", filepath);
        if (bufFill > 0)
                fatalf("Unconsumed characters at the end!\n");
        fclose(f);

        edit->cursorBytePosition = 0;
        edit->cursorCodepointPosition = 0;
}
