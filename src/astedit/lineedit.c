#include <astedit/astedit.h>
#include <astedit/utf8.h>
#include <astedit/bytes.h>
#include <astedit/lineedit.h>

void LineEdit_set_contents_from_string(struct LineEdit *lineEdit, const char *string, int length)
{
        if (length + 1 > sizeof lineEdit->buf) //XXX
                return;  // XXX: buffer too small!
        copy_memory(lineEdit->buf, string, length);
        lineEdit->fill = length;
        lineEdit->buf[lineEdit->fill] = 0;
        lineEdit->cursorBytePosition = length;
}

void LineEdit_insert_codepoint(uint32_t codepoint, struct LineEdit *lineEdit)
{
        char tmp[4];
        int r = encode_codepoint_as_utf8(codepoint, tmp, 0, 4);
        ENSURE(r > 0); // TODO: handle encode error
        if (lineEdit->fill + r + 1 > sizeof lineEdit->buf)
                return;  // XXX: buffer too small!
        move_memory(lineEdit->buf + lineEdit->cursorBytePosition,
                r, lineEdit->fill - lineEdit->cursorBytePosition);
        copy_memory(lineEdit->buf + lineEdit->cursorBytePosition, tmp, r);
        lineEdit->cursorBytePosition += r;
        lineEdit->fill += r;
        lineEdit->buf[lineEdit->fill] = 0;
}

void LineEdit_erase_backwards(struct LineEdit *lineEdit)
{
        if (lineEdit->cursorBytePosition == 0)
                return;
        int i = lineEdit->cursorBytePosition;
        do
                i--;
        while (i > 0 && !is_utf8_leader_byte(lineEdit->buf[i]));
        int deletedBytes = lineEdit->cursorBytePosition - i;
        move_memory(lineEdit->buf + lineEdit->cursorBytePosition,
                -deletedBytes, lineEdit->fill - lineEdit->cursorBytePosition);
        lineEdit->fill -= deletedBytes;
        lineEdit->buf[lineEdit->fill] = 0;
        lineEdit->cursorBytePosition = i;
}

void LineEdit_erase_forwards(struct LineEdit *lineEdit)
{
        if (lineEdit->cursorBytePosition == lineEdit->fill)
                return;
        int i = lineEdit->cursorBytePosition;
        do
                i++;
        while (i < lineEdit->fill &&!is_utf8_leader_byte(lineEdit->buf[i]));
        int deletedBytes = i - lineEdit->cursorBytePosition;
        move_memory(lineEdit->buf + i, -deletedBytes, lineEdit->fill - i);
        lineEdit->fill -= deletedBytes;
        lineEdit->buf[lineEdit->fill] = 0;
}

void LineEdit_move_cursor_to_beginning(struct LineEdit *lineEdit)
{
        lineEdit->cursorBytePosition = 0;
}

void LineEdit_move_cursor_to_end(struct LineEdit *lineEdit)
{
        lineEdit->cursorBytePosition = lineEdit->fill;
}

void LineEdit_move_cursor_left(struct LineEdit *lineEdit)
{
        if (lineEdit->cursorBytePosition == 0)
                return;
        int i = lineEdit->cursorBytePosition;
        do
                i--;
        while (i > 0 && !is_utf8_leader_byte(lineEdit->buf[i]));
        lineEdit->cursorBytePosition = i;
}

void LineEdit_move_cursor_right(struct LineEdit *lineEdit)
{
        if (lineEdit->cursorBytePosition == lineEdit->fill)
                return;
        int i = lineEdit->cursorBytePosition;
        do
                i++;
        while (i < lineEdit->fill && !is_utf8_leader_byte(lineEdit->buf[i]));
        lineEdit->cursorBytePosition = i;
}
