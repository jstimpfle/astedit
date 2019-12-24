#include <astedit/textedit.h>
#include <astedit/regex.h>
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

static FILEPOS get_textropereadbuffer_filepos(struct TextropeReadBuffer *buffer)
{
        return buffer->readpos
                - buffer->numBufferedBytes
                + buffer->numConsumedBytes;
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

static void search_next_with_pattern(struct TextEdit *edit, struct MatchCtx *matchCtx)
{
        struct TextropeReadBuffer readBuffer;

        {
        FILEPOS startPos;
        if (matchCtx->haveMatch)
                startPos = matchCtx->matchEndPos;
        else
                startPos = edit->cursorBytePosition;
        reset_TextropeReadBuffer(&readBuffer, edit->rope, startPos);
        }

        for (;;) {
                log_postf("read from pos %d", (int) get_textropereadbuffer_filepos(&readBuffer));
                int c = read_next_character_from_rope(&readBuffer);
                if (c == -1)
                        break;
                feed_character_into_regex_search(matchCtx, c);
                if (matchCtx->haveMatch)
                        break;
        }
        if (matchCtx->haveMatch) {
                        FILEPOS matchEndPos = get_textropereadbuffer_filepos(&readBuffer);
                        FILEPOS matchStartPos = matchEndPos - 1; // TODO need way to figure out match length
                        matchCtx->matchStartPos = matchStartPos;
                        matchCtx->matchEndPos = matchEndPos;
                        send_notification_to_textedit_f(edit, NOTIFICATION_INFO, "MATCH at pos %d!\n", matchStartPos);
        }
        else {
                        send_notification_to_textedit_f(edit, NOTIFICATION_ERROR, "NOT FOUND!\n");
        }
}

static struct RegexReadCtx readCtx;
static struct MatchCtx matchCtx;
static int isMatchingInitialized;

void setup_search(struct TextEdit *edit, const char *pattern, int length)
{
        if (isMatchingInitialized)
                teardown_search(edit); //XXX: should we rather die?
        log_postf("Search started for pattern '%s'", pattern);
        setup_readctx(&readCtx, pattern);
        read_pattern(&readCtx);
        if (readCtx.bad) {
                send_notification_to_textedit_f(edit, NOTIFICATION_ERROR, "Bad pattern.");
                teardown_readctx(&readCtx);
        }
        else {
                setup_matchctx_from_readctx(&matchCtx, &readCtx);
                isMatchingInitialized = 1;
        }
}

int search_next_match(struct TextEdit *edit, FILEPOS *matchStart, FILEPOS *matchEnd)
{
        if (!isMatchingInitialized)
                return 0;
        log_postf("Continue search");
        search_next_with_pattern(edit, &matchCtx);
        return extract_current_match(&matchCtx, matchStart, matchEnd);  // XXX just forwarding? really bad code!
}

void teardown_search(struct TextEdit *edit)
{
        UNUSED(edit);
        if (isMatchingInitialized) {
                teardown_matchctx(&matchCtx);
                teardown_readctx(&readCtx);
                isMatchingInitialized = 0;
        }
}
