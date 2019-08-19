#ifndef TEXTROPEUTF8DECODE_H_INCLUDED
#define TEXTROPEUTF8DECODE_H_INCLUDED

#ifndef ASTEDIT_FILEPOSITIONS_H_INCLUDED
#include <astedit/filepositions.h>
#endif

struct TextropeUTF8Decoder {
        struct Textrope *rope;
        char *buffer;
        int bufferStart;
        int bufferEnd;
        FILEPOS readPosition;
};

void init_UTF8Decoder(struct TextropeUTF8Decoder *decoder, struct Textrope *rope, FILEPOS initialPosInBytes);
void exit_UTF8Decoder(struct TextropeUTF8Decoder *decoder);
uint32_t read_codepoint_from_UTF8Decoder(struct TextropeUTF8Decoder *decoder);

static inline FILEPOS readpos_in_bytes_of_UTF8Decoder(struct TextropeUTF8Decoder *decoder)
{
        return decoder->readPosition
                - decoder->bufferEnd
                + decoder->bufferStart;
}

#endif
