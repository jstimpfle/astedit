#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/utf8.h>

int get_utf8_sequence_length_from_leader_byte(int c)
{
        if (!(c & 0x80))
                return 1;
        if (!(c & 0xc0))
                return 2;
        if (!(c & 0xe0))
                return 3;
        return 4;
}

int encode_codepoint_as_utf8(unsigned codepoint, char *str, int start, int end)
{
        unsigned char *s = (void *)&str[start]; //XXX
        if (codepoint < (1 << 7)) {
                if (start >= end)
                        return 0;
                s[0] = (unsigned char)codepoint;
                return 1;
        }
        if (codepoint < (1 << 11)) {
                if (start + 1 >= end)
                        return 0;
                s[0] = (unsigned char)((codepoint >> 6) | 0xc0);
                s[1] = (unsigned char)((codepoint & ~0xc0) | 0x80);
                return 2;
        }
        if (codepoint < (1 << 16)) {
                if (start + 2 >= end)
                        return 0;
                s[0] = (unsigned char)((codepoint >> 12) | 0xe0);
                s[1] = (unsigned char)(((codepoint >> 6) & ~0xc0) | 0x80);
                s[2] = (unsigned char)((codepoint        & ~0xc0) | 0x80);
                return 3;
        }
        if (codepoint < (1 << 21)) {
                if (start + 3 >= end)
                        return 0;
                s[0] = (unsigned char)((codepoint >> 18) | 0xf0);
                s[1] = (unsigned char)(((codepoint >> 12) & ~0xc0) | 0x80);
                s[1] = (unsigned char)(((codepoint >> 6) & ~0xc0) | 0x80);
                s[1] = (unsigned char)((codepoint         & ~0xc0) | 0x80);
                return 4;
        }
        return 0;
}

int decode_codepoint_from_utf8(const char *str, int start, int end, int *out_next, unsigned *out_codepoint)
{
        const unsigned char *s = (void *)&str[start];  //XXX
        if (start >= end)
                return 0;
        if (s[0] >> 7 == 0) {
                *out_next = start + 1;
                *out_codepoint = s[0];
                return 1;
        }
        else if ((s[0] & 0xe0) == 0xc0) {
                if (start + 1 >= end)
                        return 0;
                if ((s[1] & 0xc0) != 0x80)
                        return -1;
                *out_next = start + 2;
                *out_codepoint = ((s[0] & ~0xe0) << 6) | (s[1] & ~0xc0);
                return 1;
        }
        else if (((s[0] & 0xf0) == 0xe0)) {
                if (start + 2 >= end)
                        return 0;
                if ((s[1] & 0xc0) != 0x80)
                        return -1;
                if ((s[2] & 0xc0) != 0x80)
                        return -1;

                start += 3;
                unsigned codepoint = ((s[0] & ~0xf0) << 12) | ((s[1] & ~0xc0) << 6)
                        | (s[2] & ~0xc0);


                /* special handling for broken input (surrogate pairs) */
                if (codepoint >= 0xDC00 && end - start >= 3) {
                        unsigned d;
                        int next;
                        int r = decode_codepoint_from_utf8(str, start, end, &next, &d);
                        if (!r)
                                return 0;
                        //if (d < 0xD800)
                          //      return 0;
                        start = next;
                        codepoint = 0x10000 + (codepoint - 0xDC00) + ((d - 0xD800) << 10);
                        codepoint = codepoint;
                }

                *out_next = start;
                *out_codepoint = codepoint;
                return 1;
        }
        else if ((s[0] & 0xf8) == 0xf0) {
                if (start + 3 >= end)
                        return 0;
                if ((s[1] & 0xc0) != 0x80)
                        return -1;
                if ((s[2] & 0xc0) != 0x80)
                        return -1;
                if ((s[3] & 0xc0) != 0x80)
                        return -1;
                *out_next = start + 4;
                *out_codepoint = ((s[0] & ~0xf8) << 18) | ((s[1] & ~0xc0) << 12)
                        | ((s[2] & ~0xc0) << 6) | (s[3] & ~0xc0);
                return 1;
        }
        else {
                return -1;
        }
}

void encode_utf8_span(const uint32_t *codepoints, int startPos, int maxPos, char *bufOut, int maxBytes,
        int *outPos, int *outNumBytes)
{
        int pos = startPos;
        int numBytes = 0;
        while (pos < maxPos && numBytes < maxBytes) {
                int r = encode_codepoint_as_utf8(codepoints[pos], bufOut, numBytes, maxBytes);
                if (!r)
                        break;
                numBytes += r;
                pos++;
        }
        *outPos = pos;
        *outNumBytes = numBytes;
}

void decode_utf8_span(const char *text, int startPos, int maxPos, uint32_t *codepointOut, int maxCodepoints,
        int *outPos, int *outNumCodepoints)
{
        int pos = startPos;
        int numCodepoints = 0;
        while (pos < maxPos && numCodepoints < maxCodepoints) {
                int r = decode_codepoint_from_utf8(text, pos, maxPos, &pos, &codepointOut[numCodepoints]);
                if (r > 0) {
                        numCodepoints++;
                }
                else if (r == -1) {
                        pos++;
                }
                else {
                        ENSURE(r == 0);
                        break;
                }
        }
        *outPos = pos;
        *outNumCodepoints = numCodepoints;
}

void decode_utf8_span_and_move_rest_to_front(char *inputText, int length,
        uint32_t *codepointOut, int *outLength, int *outNumCodepoints)
{
        int pos;
        decode_utf8_span(inputText, 0, length, codepointOut, length /*XXX*/,
                &pos, outNumCodepoints);
        int remainingBytes = length - pos;
        ENSURE(0 <= remainingBytes && remainingBytes < 4);
        move_memory(inputText + pos, -pos, remainingBytes);
        *outLength = remainingBytes;
}
