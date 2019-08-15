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

void init_TextropeReadBuffer(struct TextropeReadBuffer *trbuf, struct Textrope *rope, int initialPosInBytes)
{
        trbuf->rope = rope;
        ALLOC_MEMORY(&trbuf->buffer, TEXTROPEREADBUFFER_CAPACITY);
        trbuf->bufferStart = 0;
        trbuf->bufferEnd = 0;
        trbuf->readPosition = initialPosInBytes;
}

void exit_TextropeReadBuffer(struct TextropeReadBuffer *trbuf)
{
        FREE_MEMORY(&trbuf->buffer);
}

static void move_bytes_in_TextropeReadBuffer_to_front(struct TextropeReadBuffer *trbuf)
{
        int bufferedBytes = trbuf->bufferEnd - trbuf->bufferStart;
        move_memory(trbuf->buffer + trbuf->bufferStart,
                -trbuf->bufferStart, bufferedBytes);
        trbuf->bufferStart = 0;
        trbuf->bufferEnd = bufferedBytes;
}

void refill_TextropeReadBuffer(struct TextropeReadBuffer *trbuf)
{
        int textropeLength = textrope_length(trbuf->rope);
        int bytesToRead = textropeLength - trbuf->readPosition;
        if (bytesToRead > TEXTROPEREADBUFFER_CAPACITY - trbuf->bufferEnd)
                bytesToRead = TEXTROPEREADBUFFER_CAPACITY - trbuf->bufferEnd;
        copy_text_from_textrope(trbuf->rope, trbuf->readPosition,
                trbuf->buffer + trbuf->bufferEnd, bytesToRead);
        trbuf->bufferEnd += bytesToRead;
        trbuf->readPosition += bytesToRead;
}

static int has_TextropeReadBuffer_more_data(struct TextropeReadBuffer *trbuf)
{
        /* if there is more data, make sure to have at least 4 bytes in the buffer (UTF-8!) */
        if (trbuf->bufferStart + 4 <= trbuf->bufferEnd)
                return 1;
        refill_TextropeReadBuffer(trbuf);
        return trbuf->bufferStart < trbuf->bufferEnd;
}

int look_byte_from_TextropeReadBuffer(struct TextropeReadBuffer *trbuf)
{
        if (!has_TextropeReadBuffer_more_data(trbuf))
                return -1;
        return trbuf->buffer[trbuf->bufferStart];
}

void consume_byte_from_TextropeReadBuffer(struct TextropeReadBuffer *trbuf)
{
        ENSURE(trbuf->bufferStart < trbuf->bufferEnd);
        trbuf->bufferStart++;
}




void init_UTF8DecodeStream(struct UTF8DecodeStream *stream, struct Textrope *textrope, int initialPosInBytes)
{
        init_TextropeReadBuffer(&stream->readbuffer, textrope, initialPosInBytes);
        stream->codepointBufferStart = 0;
        stream->codepointBufferEnd = 0;
};

void exit_UTF8DecodeStream(struct UTF8DecodeStream *stream)
{
        exit_TextropeReadBuffer(&stream->readbuffer);
}

void refill_UTF8DecodeStream(struct UTF8DecodeStream *stream)
{
        struct TextropeReadBuffer *trbuf = &stream->readbuffer;
        if (TEXTROPEREADBUFFER_CAPACITY - trbuf->bufferEnd < 16)
                /* 16 = arbitrary small number >= 4 (max UTF-8 sequence length) */
                move_bytes_in_TextropeReadBuffer_to_front(trbuf);
        refill_TextropeReadBuffer(trbuf);
        if (trbuf->bufferStart == trbuf->bufferEnd)
                return;
        if (stream->codepointBufferStart == stream->codepointBufferEnd) {
                stream->codepointBufferStart = 0;
                stream->codepointBufferEnd = 0;
        }
        int numCodepointsDecoded;
        decode_utf8_span(
                trbuf->buffer, trbuf->bufferStart, trbuf->bufferEnd,
                stream->codepointBuffer + stream->codepointBufferStart,
                LENGTH(stream->codepointBuffer) - stream->codepointBufferStart,
                &trbuf->bufferStart, &numCodepointsDecoded);
        stream->codepointBufferEnd += numCodepointsDecoded;
}

int has_UTF8DecodeStream_more_data(struct UTF8DecodeStream *stream)
{
        if (stream->codepointBufferStart < stream->codepointBufferEnd)
                return 1;
        refill_UTF8DecodeStream(stream);
        return stream->codepointBufferStart < stream->codepointBufferEnd;
}

uint32_t look_codepoint_from_UTF8DecodeStream(struct UTF8DecodeStream *stream)
{
        if (!has_UTF8DecodeStream_more_data(stream))
                return (uint32_t) -1;
        return stream->codepointBuffer[stream->codepointBufferStart];
}

void consume_codepoint_from_UTF8DecodeStream(struct UTF8DecodeStream *stream)
{
        ENSURE(stream->codepointBufferStart < stream->codepointBufferEnd);
        stream->codepointBufferStart++;
}