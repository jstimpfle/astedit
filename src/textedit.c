#include <astedit/astedit.h>
#include <astedit/memoryalloc.h>
#include <astedit/utf8.h>
#include <astedit/textrope.h>
#include <astedit/textedit.h>
#include <string.h>  // XXX memcpy()

// TEST
static struct Textrope *textrope;


void init_TextEdit(struct TextEdit *edit)
{
        edit->contents = NULL;
        edit->length = 0;

        textrope = create_textrope();
}

void exit_TextEdit(struct TextEdit *edit)
{
        FREE_MEMORY(&edit->contents);
        edit->length = 0;

        destroy_textrope(textrope);
}


static void append_codepoint_to_TextEdit(struct TextEdit *edit, unsigned long codepoint)
{
        char tmp[16];
        int numBytes = encode_codepoint_as_utf8(codepoint, &tmp[0], 0, sizeof tmp);
        tmp[numBytes] = 0;  // for nicer debugging
        int pos = edit->length;
        edit->length += numBytes;
        REALLOC_MEMORY(&edit->contents, edit->length);
        memcpy(edit->contents + pos, &tmp[0], numBytes);

        insert_text_into_textrope(textrope, textrope_length(textrope), &tmp[0], numBytes);

        debug_print_textrope(textrope);
}

static void pop_character_from_TextEdit_if_possible(struct TextEdit *edit)
{
        // currently not possible since we're using rope now
        return;

        int length = edit->length;
        if (length == 0)
                return;
        length--;
        /* UTF-8 magic */
        for (;;) {
                unsigned c = (unsigned char) edit->contents[length];
                if ((c & 0xc0) != 0x80)
                        break;
                ENSURE(length > 0);
                length--;
        }
        edit->length = length;
}

void process_input_in_textEdit(struct Input *input, struct TextEdit *edit)
{
        if (input->inputKind == INPUT_KEY) {
                switch (input->tKey.keyKind) {
                case KEY_ENTER:
                        append_codepoint_to_TextEdit(edit, 0x0a);
                        break;
                case KEY_BACKSPACE:
                        pop_character_from_TextEdit_if_possible(edit);
                        break;
                default:
                        if (input->tKey.hasCodepoint) {
                                unsigned long codepoint = input->tKey.codepoint;
                                append_codepoint_to_TextEdit(edit, codepoint);
                        }
                        break;
                }


        }
}