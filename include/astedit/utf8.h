#ifndef ASTEDIT_UTF8_H_INCLUDED
#define ASTEDIT_UTF8_H_INCLUDED

#include <stdint.h>
static inline int is_utf8_leader_byte(int c) { return (c & 0xc0) != 0x80; }
int encode_codepoint_as_utf8(unsigned codepoint, char *str, int start, int end);
int decode_codepoint_from_utf8(const char *str, int start, int end, int *out_next, unsigned *out_codepoint);
void decode_utf8_span(const char *text, int startPos, int maxPos, uint32_t *codepointOut, int maxCodepoints,
        int *outPos, int *outNumCodepoints);

#endif
