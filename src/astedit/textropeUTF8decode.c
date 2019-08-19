#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/textrope.h>
#include <astedit/memoryalloc.h>
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

static void move_bytes_in_TextropeReadBuffer_to_front(struct TextropeUTF8Decoder *decoder)
{
        int bufferedBytes = decoder->bufferEnd - decoder->bufferStart;
        move_memory(decoder->buffer + decoder->bufferStart,
                -decoder->bufferStart, bufferedBytes);
        decoder->bufferStart = 0;
        decoder->bufferEnd = bufferedBytes;
}

static void refill_UTF8Decoder(struct TextropeUTF8Decoder *decoder)
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
                move_bytes_in_TextropeReadBuffer_to_front(decoder);
                refill_UTF8Decoder(decoder);
        }
        
        uint32_t codepoint;
        int r = decode_codepoint_from_utf8(decoder->buffer,
                decoder->bufferStart, decoder->bufferEnd,
                &decoder->bufferStart, &codepoint);
        if (r == 0)
                return (uint32_t) -1;
        if (r == -1)
                return (uint32_t) '?'; //XXX
        return codepoint;
}