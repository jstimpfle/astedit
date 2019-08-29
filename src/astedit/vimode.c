#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/utf8.h>
#include <astedit/logging.h>
#include <astedit/osthread.h>
#include <astedit/filereadwritethread.h>
#include <astedit/vimode.h>
#include <astedit/textedit.h>
#include <astedit/texteditloadsave.h>

#include <string.h>  //strlen()

void interpret_cmdline(struct ViCmdline *cmdline, struct TextEdit *edit)
{
        log_begin();
        log_writef("Got cmdline: ");
        log_write(cmdline->buf, cmdline->fill);
        log_end();

        // XXX parsing not nice.
        if (cmdline->buf[0] == 'w' && cmdline->buf[1] == ' ') {
                const char *filepath = cmdline->buf + 2;
                //XXX not zero terminated
                int filepathLen = (int) strlen(filepath);
                write_contents_from_textedit_to_file(edit, filepath, filepathLen);
        }

        if (cmdline->fill == 1 && cmdline->buf[0] == 'q') {
                shouldWindowClose = 1;
        }
}


void reset_ViCmdline(struct ViCmdline *cmdline)
{
        ZERO_MEMORY(cmdline);
}

void insert_codepoint_in_ViCmdline(uint32_t codepoint, struct ViCmdline *cmdline)
{
        char tmp[4];
        int r = encode_codepoint_as_utf8(codepoint, tmp, 0, 4);
        ENSURE(r > 0); // TODO: handle encode error
        if (cmdline->fill + r > sizeof cmdline->buf)
                return;  // XXX: buffer too small!
        move_memory(cmdline->buf + cmdline->cursorBytePosition,
                r, cmdline->fill - cmdline->cursorBytePosition);

        log_postf("ok\n");
        copy_memory(cmdline->buf + cmdline->cursorBytePosition, tmp, r);
        cmdline->cursorBytePosition += r;
        cmdline->fill += r;
}

void erase_backwards_in_ViCmdline(struct ViCmdline *cmdline)
{
        if (cmdline->cursorBytePosition == 0)
                return;
        int i = cmdline->cursorBytePosition;
        do
                i--;
        while (i > 0 && !is_utf8_leader_byte(cmdline->buf[i]));
        int deletedBytes = cmdline->cursorBytePosition - i;
        move_memory(cmdline->buf + cmdline->cursorBytePosition,
                -deletedBytes, cmdline->fill - cmdline->cursorBytePosition);
        cmdline->fill -= deletedBytes;
        cmdline->cursorBytePosition = i;
}

void erase_forwards_in_ViCmdline(struct ViCmdline *cmdline)
{
        if (cmdline->cursorBytePosition == cmdline->fill)
                return;
        int i = cmdline->cursorBytePosition;
        do
                i++;
        while (i < cmdline->fill &&!is_utf8_leader_byte(cmdline->buf[i]));
        int deletedBytes = i - cmdline->cursorBytePosition;
        move_memory(cmdline->buf + i, -deletedBytes, cmdline->fill - i);
        cmdline->fill -= deletedBytes;
}

void move_cursor_to_beginning_in_cmdline(struct ViCmdline *cmdline)
{
        cmdline->cursorBytePosition = 0;
}

void move_cursor_to_end_in_cmdline(struct ViCmdline *cmdline)
{
        cmdline->cursorBytePosition = cmdline->fill;
}

void move_cursor_left_in_cmdline(struct ViCmdline *cmdline)
{
        if (cmdline->cursorBytePosition == 0)
                return;
        int i = cmdline->cursorBytePosition;
        do
                i--;
        while (i > 0 && !is_utf8_leader_byte(cmdline->buf[i]));
        cmdline->cursorBytePosition = i;
}

void move_cursor_right_in_cmdline(struct ViCmdline *cmdline)
{
        if (cmdline->cursorBytePosition == cmdline->fill)
                return;
        int i = cmdline->cursorBytePosition;
        do
                i++;
        while (i < cmdline->fill && !is_utf8_leader_byte(cmdline->buf[i]));
        cmdline->cursorBytePosition = i;
}
