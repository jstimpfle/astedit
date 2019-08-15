#ifndef TEXTROPEUTF8DECODE_H_INCLUDED
#define TEXTROPEUTF8DECODE_H_INCLUDED


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


enum { UTF8DECODESTREAM_CODEPOINTBUFFERSIZE = 64 };  // enough?

struct UTF8DecodeStream {
        struct TextropeReadBuffer readbuffer;
        uint32_t codepointBuffer[UTF8DECODESTREAM_CODEPOINTBUFFERSIZE];
        int codepointBufferStart;
        int codepointBufferEnd;
};

void init_UTF8DecodeStream(struct UTF8DecodeStream *stream, struct Textrope *textrope, int initialPosInBytes);
void exit_UTF8DecodeStream(struct UTF8DecodeStream *stream);
void refill_UTF8DecodeStream(struct UTF8DecodeStream *stream);
int has_UTF8DecodeStream_more_data(struct UTF8DecodeStream *stream);
uint32_t look_codepoint_from_UTF8DecodeStream(struct UTF8DecodeStream *stream);
void consume_codepoint_from_UTF8DecodeStream(struct UTF8DecodeStream *stream);


#endif
