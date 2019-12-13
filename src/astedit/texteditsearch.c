#include <astedit/textedit.h>
#include <astedit/search.h>
#include <astedit/filepositions.h>
#include <astedit/logging.h>
#include <astedit/texteditsearch.h>

struct TextropeReadBuffer {
        unsigned char buffer[1024];
        int numBufferedBytes;
        int numConsumedBytes;  // of buffered bytes
        struct Textrope *rope;
        FILEPOS readpos;
};

static void reset_TextropeReadBuffer(struct TextropeReadBuffer *buffer, struct Textrope *rope, FILEPOS readpos)
{
        buffer->numBufferedBytes = 0;
        buffer->numConsumedBytes = 0;
        buffer->readpos = readpos;
        buffer->rope = rope;
}

static int read_next_character_from_rope(struct TextropeReadBuffer *buffer)
{
        if (buffer->numConsumedBytes == buffer->numBufferedBytes) {
                FILEPOS numBytesToRead = textrope_length(buffer->rope) - buffer->readpos;
                if (numBytesToRead == 0)
                        return -1; /*EOF*/
                if (numBytesToRead > LENGTH(buffer->buffer))
                        numBytesToRead = LENGTH(buffer->buffer);
                copy_text_from_textrope(buffer->rope, buffer->readpos, (char*)buffer->buffer, numBytesToRead);
                buffer->readpos += numBytesToRead;
                buffer->numBufferedBytes = cast_filepos_to_int(numBytesToRead);
                buffer->numConsumedBytes = 0;
        }
        return buffer->buffer[buffer->numConsumedBytes++];
}

static void search_next_with_pattern(struct TextEdit *edit, struct CompiledPattern *pattern)
{
        struct TextropeReadBuffer buffer;
        struct MatchState matchState;
        {
                //XXX TODO: what if there is no next codepoint?
                FILEPOS readpos = edit->cursorBytePosition;
                reset_TextropeReadBuffer(&buffer, edit->rope, readpos);
                init_pattern_match(pattern, &matchState, readpos);
        }

        for (;;) {
                int character = read_next_character_from_rope(&buffer);
                if (character == -1)
                        break;
                feed_character_into_search(&matchState, character);
                if (matchState.earliestMatch != -1)
                        break;
        }
#define SEND(x, y, z) send_notification_to_textedit(x, y, z, sizeof z - 1)
        if (matchState.earliestMatch != -1) {
                        SEND(edit, NOTIFICATION_INFO, "MATCH!\n");
                        /*XXX need clean way to swithc on selection */
                        move_cursor_to_byte_position(edit, matchState.earliestMatch, 0);
                        move_cursor_to_byte_position(edit, matchState.bytePosition, 1);
        }
        else {
                        SEND(edit, NOTIFICATION_ERROR, "NOT FOUND!\n");
        }
#undef SEND
}

static struct CompiledPattern compiledPattern;

void start_search(struct TextEdit *edit, const char *pattern, int length)
{
        log_postf("Search started for %s", pattern);
        compile_pattern_from_fixed_string(&compiledPattern, pattern, length);
        search_next_with_pattern(edit, &compiledPattern);
}

void continue_search(struct TextEdit *edit)
{
        search_next_with_pattern(edit, &compiledPattern);
}

void end_search(struct TextEdit *edit)
{
        UNUSED(edit);
        free_pattern(&compiledPattern);
}
