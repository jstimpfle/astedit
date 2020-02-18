#ifndef TEXTROPEUTF8DECODE_H_INCLUDED
#define TEXTROPEUTF8DECODE_H_INCLUDED

#ifndef ASTEDIT_FILEPOSITIONS_H_INCLUDED
#include <astedit/filepositions.h>
#endif

struct TextropeUTF8Decoder {
        struct Textrope *rope;
        int bufferStart;
        int bufferEnd;
        FILEPOS readPosition;
        char buffer[1024];
};

void reset_UTF8Decoder(struct TextropeUTF8Decoder *trbuf, struct Textrope *rope, FILEPOS initialPosInBytes);
uint32_t read_codepoint_from_UTF8Decoder(struct TextropeUTF8Decoder *decoder);

static inline FILEPOS readpos_in_bytes_of_UTF8Decoder(struct TextropeUTF8Decoder *decoder)
{
        return decoder->readPosition
                - decoder->bufferEnd
                + decoder->bufferStart;
}

#endif
