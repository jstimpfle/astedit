#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/textrope.h>
#include <astedit/logging.h>
#include <astedit/memory.h>
#include <astedit/utf8.h>
#include <astedit/textropeUTF8decode.h>


enum {
        // maybe a little large but I think this is the current node size in the rope
        TEXTROPEREADBUFFER_CAPACITY = 1024,
};

void init_UTF8Decoder(struct TextropeUTF8Decoder *trbuf, struct Textrope *rope, FILEPOS initialPosInBytes)
{
        trbuf->rope = rope;
        ALLOC_MEMORY(&trbuf->buffer, TEXTROPEREADBUFFER_CAPACITY);
        trbuf->bufferStart = 0;
        trbuf->bufferEnd = 0;
        trbuf->readPosition = initialPosInBytes;
}

void exit_UTF8Decoder(struct TextropeUTF8Decoder *decoder)
{
        FREE_MEMORY(&decoder->buffer);
}

static void UTF8Decoder_move_bytes_to_front(struct TextropeUTF8Decoder *decoder)
{
        int bufferedBytes = decoder->bufferEnd - decoder->bufferStart;
        move_memory(decoder->buffer + decoder->bufferStart,
                -decoder->bufferStart, bufferedBytes);
        decoder->bufferStart = 0;
        decoder->bufferEnd = bufferedBytes;
}

static void UTF8Decoder_refill(struct TextropeUTF8Decoder *decoder)
{
        FILEPOS textropeLength = textrope_length(decoder->rope);
        int bytesToRead = min_filepos_as_int(
                filepos_sub(textropeLength, decoder->readPosition),
                TEXTROPEREADBUFFER_CAPACITY - decoder->bufferEnd
        );
        copy_text_from_textrope(decoder->rope, decoder->readPosition,
                decoder->buffer + decoder->bufferEnd, bytesToRead);
        decoder->bufferEnd += bytesToRead;
        decoder->readPosition += bytesToRead;
}

uint32_t read_codepoint_from_UTF8Decoder(struct TextropeUTF8Decoder *decoder)
{
        if (decoder->bufferEnd - decoder->bufferStart < 4) {
                UTF8Decoder_move_bytes_to_front(decoder);
                UTF8Decoder_refill(decoder);
        }

        uint32_t codepoint;
        int r = decode_codepoint_from_utf8(decoder->buffer,
                decoder->bufferStart, decoder->bufferEnd,
                &decoder->bufferStart, &codepoint);
        if (r == 0) {
                /* Not enough bytes in buffer. Since we made sure above that at
                 * least 4 bytes are in the buffer if so many are available,
                 * that means that we're at the end of the stream. */
                ENSURE(decoder->bufferEnd - decoder->bufferStart < 4 &&
                       decoder->readPosition == textrope_length(decoder->rope));
                /* If there are unconsumed final characters (unfinished UTF-8
                 * sequence at the end of the stream - which can happen easily
                 * with binary text), we'll consume them to allow us to arrive
                 * at the end of the stream. */
                if (decoder->bufferStart < decoder->bufferEnd) {
                        decoder->bufferStart = decoder->bufferEnd;
                        return (uint32_t) 0xFFFD;
                }
                /* Otherwise, return -1 to indicate regular end of stream (no
                 * encoding errors). */
                return (uint32_t) -1;
        }
        if (r == -1) {
                /* Invalid UTF-8 */
                /* Just eat one (invalid) byte to allow progress.
                 * Alternatively, we could try to find the start of the next
                 * UTF-8 sequence. */
                if (decoder->bufferStart < decoder->bufferEnd)
                        decoder->bufferStart ++;
                return (uint32_t) 0xFFFD;
        }
        return codepoint;
}
