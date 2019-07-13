#ifndef ASTEDIT_UTF8_H_INCLUDED
#define ASTEDIT_UTF8_H_INCLUDED

static inline int is_utf8_leader_byte(int c) { return (c & 0xc0) != 0x80; }
int encode_codepoint_as_utf8(unsigned codepoint, char *str, int start, int end);
int decode_codepoint_from_utf8(const char *str, int start, int end, int *out_next, unsigned *out_codepoint);

#endif
