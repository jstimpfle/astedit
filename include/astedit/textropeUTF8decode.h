#ifndef TEXTROPEUTF8DECODE_H_INCLUDED
#define TEXTROPEUTF8DECODE_H_INCLUDED

/* TODO: merge the two structs in here. TextropeReadBuffer
is used only by UTF8DecodeStream, and the latter doesn't
really add much value on top */

struct TextropeReadBuffer {
        struct Textrope *rope;
        char *buffer;
        int bufferStart;
        int bufferEnd;
        int readPosition;
};

void init_TextropeReadBuffer(struct TextropeReadBuffer *trbuf, struct Textrope *rope, int initialPosInBytes);
void exit_TextropeReadBuffer(struct TextropeReadBuffer *trbuf);
void refill_TextropeReadBuffer(struct TextropeReadBuffer *trbuf);
int look_byte_from_TextropeReadBuffer(struct TextropeReadBuffer *trbuf);
void consume_byte_from_TextropeReadBuffer(struct TextropeReadBuffer *trbuf);


struct UTF8DecodeStream {
        struct TextropeReadBuffer readbuffer;
};

void init_UTF8DecodeStream(struct UTF8DecodeStream *stream, struct Textrope *textrope, int initialPosInBytes);
void exit_UTF8DecodeStream(struct UTF8DecodeStream *stream);
int has_UTF8DecodeStream_more_data(struct UTF8DecodeStream *stream);
uint32_t read_codepoint_from_UTF8DecodeStream(struct UTF8DecodeStream *stream);

static inline int readpos_in_bytes_of_UTF8Decoder(struct UTF8DecodeStream *stream)
{
        return stream->readbuffer.readPosition
                - stream->readbuffer.bufferEnd
                + stream->readbuffer.bufferStart;
}

#endif
