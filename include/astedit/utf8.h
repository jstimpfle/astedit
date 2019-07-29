#ifndef ASTEDIT_UTF8_H_INCLUDED
#define ASTEDIT_UTF8_H_INCLUDED

#include <stdint.h>
static inline int is_utf8_leader_byte(int c) { return (c & 0xc0) != 0x80; }

int get_utf8_sequence_length_from_leader_byte(int c);

int encode_codepoint_as_utf8(unsigned codepoint, char *str, int start, int end);
int decode_codepoint_from_utf8(const char *str, int start, int end, int *out_next, unsigned *out_codepoint);

void encode_utf8_span(const uint32_t *codepoints, int startPos, int maxPos, char *bufOut, int maxBytes,
        int *outPos, int *outNumBytes);
void decode_utf8_span(const char *text, int startPos, int maxPos, uint32_t *codepointOut, int maxCodepoints,
        int *outPos, int *outNumCodepoints);


/* This version is intended to be used with fixed-size read buffers.
As many characters as possible (but at most `length`) characters from the
input read buffer are decoded. The output is stored in the codepointOut
array, which must be at least of size `length` (otherwise there is a possible
buffer overflow).
Since there are always less than 4 remaining bytes (UTF-8 byte sequences are
of length <= 4), we can afford to move them to the front. The number of
remaining bytes is stored in *outLength.
*/
void decode_utf8_span_and_move_rest_to_front(char *inputText, int length,
        uint32_t *codepointOut, int *outLength, int *outNumCodepoints);

#endif
