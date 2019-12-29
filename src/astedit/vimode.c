#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/utf8.h>
#include <astedit/logging.h>
#include <astedit/textedit.h>
#include <astedit/texteditloadsave.h>
#include <astedit/texteditsearch.h>
#include <astedit/textpositions.h>
#include <astedit/vimode.h>
#include <string.h>
#include <stdlib.h> // strtol()

void setup_vistate(struct ViState *vistate)
{
        vistate->vimodeKind = VIMODE_NORMAL;
        vistate->modalKind = VIMODAL_NORMAL;
        vistate->moveModalKind = VIMOVEMODAL_NONE;
        ZERO_MEMORY(&vistate->cmdline);
}

void teardown_vistate(struct ViState *vistate)
{
        // currently nothing. At some point, there will be allocations.
        UNUSED(vistate);
}

void interpret_cmdline(struct ViCmdline *cmdline, struct TextEdit *edit)
{
        /*
        log_begin();
        log_writef("Got cmdline: '");
        log_write(cmdline->buf, cmdline->fill);
        log_writef("'");
        log_end();
        */

        // XXX parsing not nice.
        if (cmdline->buf[0] == 'g' && cmdline->buf[1] == ' ') {
                FILEPOS numberOfLines = textrope_number_of_lines(edit->rope);
                FILEPOS gotoLine = strtol(cmdline->buf + 2, NULL, 0) - 1;
                if (gotoLine < 0)
                        gotoLine = 0;
                else if (gotoLine > numberOfLines)
                        gotoLine = numberOfLines;
                //log_postf("go to line: %ld\n", (long) gotoLine);
                move_cursor_to_line(edit, gotoLine, 0);
        }
        else if (cmdline->buf[0] == 'r' && cmdline->buf[1] == ' ') {
                const char *filepath = cmdline->buf + 2;
                int filepathLen = cmdline->fill - 2;
                load_file_to_textedit(&edit->loading, filepath, filepathLen, edit);
        }
        else if (cmdline->buf[0] == 'w') {
                const char *filepath = NULL;
                int filepathLen = 0;
                if (cmdline->fill == 1 && edit->filepath != NULL) {
                        filepath = edit->filepath;
                        filepathLen = (int)strlen(edit->filepath);
                }
                else if (cmdline->buf[1] == ' ') {
                        filepath = cmdline->buf + 2;
                        filepathLen = cmdline->fill - 2;
                }
                if (filepath != NULL)
                        write_textrope_contents_to_file(&edit->saving, edit->rope, filepath, filepathLen);
        }
        else if (cmdline->buf[0] == '/') {
                const char *buf = cmdline->buf;
                int length = cmdline->fill;
                int start = 1;
                int end = 1;
                while (end < length && buf[end] != '/')
                        end++;
                setup_search(edit, cmdline->buf + start, end - start);
                // search for the first match right away.
                move_cursor_to_next_match(edit, edit->isSelectionMode);
        }
        else if (cmdline->buf[0] == 'q' && cmdline->fill == 1) {
                shouldWindowClose = 1;
        }
}

void clear_ViCmdline(struct ViCmdline *cmdline)
{
        cmdline->fill = 0;
        cmdline->buf[cmdline->fill] = 0;
        cmdline->cursorBytePosition = 0;
        cmdline->isAborted = 0;
        cmdline->isConfirmed = 0;
        cmdline->isNavigatingHistory = 0;
}

void set_ViCmdline_contents_from_string(struct ViCmdline *cmdline, const char *string, int length)
{
        if (length + 1 > sizeof cmdline->buf) //XXX
                return;  // XXX: buffer too small!
        copy_memory(cmdline->buf, string, length);
        cmdline->fill = length;
        cmdline->buf[cmdline->fill] = 0;
        cmdline->cursorBytePosition = length;
}

void insert_codepoint_in_ViCmdline(uint32_t codepoint, struct ViCmdline *cmdline)
{
        char tmp[4];
        int r = encode_codepoint_as_utf8(codepoint, tmp, 0, 4);
        ENSURE(r > 0); // TODO: handle encode error
        if (cmdline->fill + r + 1 > sizeof cmdline->buf)
                return;  // XXX: buffer too small!
        move_memory(cmdline->buf + cmdline->cursorBytePosition,
                r, cmdline->fill - cmdline->cursorBytePosition);
        copy_memory(cmdline->buf + cmdline->cursorBytePosition, tmp, r);
        cmdline->cursorBytePosition += r;
        cmdline->fill += r;
        cmdline->buf[cmdline->fill] = 0;
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
        cmdline->buf[cmdline->fill] = 0;
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
        cmdline->buf[cmdline->fill] = 0;
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
