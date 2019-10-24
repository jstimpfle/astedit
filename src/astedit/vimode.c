#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/utf8.h>
#include <astedit/logging.h>
#include <astedit/osthread.h>
#include <astedit/filereadwritethread.h>
#include <astedit/vimode.h>
#include <astedit/textedit.h>
#include <astedit/texteditloadsave.h>

void setup_vistate(struct ViState *vistate)
{
        vistate->vimodeKind = VIMODE_NORMAL;
        vistate->modalKind = VIMODAL_NORMAL;
        ZERO_MEMORY(&vistate->cmdline);
}

void teardown_vistate(struct ViState *vistate)
{
        // currently nothing. At some point, there will be allocations.
        UNUSED(vistate);
}

void interpret_cmdline(struct ViCmdline *cmdline, struct TextEdit *edit)
{
        log_begin();
        log_writef("Got cmdline: ");
        log_write(cmdline->buf, cmdline->fill);
        log_end();

        // XXX parsing not nice.
        if (cmdline->buf[0] == 'r' && cmdline->buf[1] == ' ') {
                const char *filepath = cmdline->buf + 2;
                int filepathLen = cmdline->fill - 2;
                load_file_to_textrope(&edit->loading, filepath, filepathLen, edit->rope);
        }
        else if (cmdline->buf[0] == 'w' && cmdline->buf[1] == ' ') {
                const char *filepath = cmdline->buf + 2;
                int filepathLen = cmdline->fill - 2;
                write_textrope_contents_to_file(&edit->saving, edit->rope, filepath, filepathLen);
        }
        else if (cmdline->fill == 1 && cmdline->buf[0] == 'q') {
                shouldWindowClose = 1;
        }
}

void clear_ViCmdline(struct ViCmdline *cmdline)
{
        cmdline->fill = 0;
        cmdline->cursorBytePosition = 0;
        cmdline->isAborted = 0;
        cmdline->isConfirmed = 0;
        cmdline->isNavigatingHistory = 0;
}

void set_ViCmdline_contents_from_string(struct ViCmdline *cmdline, const char *string, int length)
{
        if (length > LENGTH(cmdline->buf) - 1) //XXX
                length = LENGTH(cmdline->buf) - 1;
        copy_string_and_zeroterminate(cmdline->buf, string, length);
        cmdline->fill = length;
        cmdline->cursorBytePosition = length;
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
