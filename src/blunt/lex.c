#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/textrope.h>
#include <blunt/lex.h>


static void fill_buffer(struct Blunt_ReadCtx *ctx)
{
        ENSURE(ctx->bufferStart + ctx->bufferLength <= sizeof ctx->buffer);

        /* move to front. Simpler than ringbuffer */
        if (ctx->bufferLength < 16 /* arbitrary, small number */) {
                move_memory(ctx->buffer + ctx->bufferStart, -ctx->bufferStart, ctx->bufferLength);
                ctx->bufferStart = 0;
        }

        int bytesToRead = sizeof ctx->buffer - (ctx->bufferStart + ctx->bufferLength);
        int bytesAvail = textrope_length(ctx->rope) - ctx->readPos;
        if (bytesToRead > bytesAvail)
                bytesToRead = bytesAvail;
        /* note that "readPos" is ignoring what's already in the buffer.
        That's why we add bufferLength here, and also why we don't advance
        readPos after copy_text_from_textrope() */
        copy_text_from_textrope(ctx->rope, ctx->readPos + ctx->bufferLength, ctx->buffer + ctx->bufferStart, bytesToRead);
        ctx->bufferLength += bytesToRead;
}

static int has_byte(struct Blunt_ReadCtx *ctx)
{
        if (ctx->bufferLength > 0)
                return 1;
        else {
                fill_buffer(ctx);
                return ctx->bufferLength > 0;
        }
}

static int look_byte(struct Blunt_ReadCtx *ctx)
{
        if (!has_byte(ctx))
                return -1;
        int c = ctx->buffer[ctx->bufferStart];
        return c;
}

static void consume_byte(struct Blunt_ReadCtx *ctx)
{
        ENSURE(ctx->bufferLength > 0);
        ctx->readPos++;
        ctx->bufferStart++;
        ctx->bufferLength--;
}

static int is_whitespace(int c)
{
        return (unsigned) c <= 32;
}

static int is_digit(int c)
{
        return '0' <= c && c <= '9';
}

/* TODO: return more information than just a token kind. */
void blunt_lex_token(struct Blunt_ReadCtx *ctx, struct Blunt_Token *outToken)
{
        enum Blunt_TokenKind tokenKind;
        int c;

        int parseStart = ctx->readPos;
        int leadingWhiteChars = 0;

        for (;;) {
                c = look_byte(ctx);
                if (c == -1)
                        break;
                if (!is_whitespace(c))
                        break;
                consume_byte(ctx);
        }
        leadingWhiteChars = ctx->readPos - parseStart;

        if (c == -1) {
                tokenKind = BLUNT_TOKEN_EOF;
        }
        else if (is_digit(c)) {
                tokenKind = BLUNT_TOKEN_INTEGER;
                for (;;) {
                        consume_byte(ctx);
                        c = look_byte(ctx);
                        if (c == -1)
                                break;
                        if (!is_digit(c))
                                break;
                }
        }
        else {
                tokenKind = BLUNT_TOKEN_NAME;
                for (;;) {
                        consume_byte(ctx);
                        c = look_byte(ctx);
                        if (c == -1)
                                break;
                        if (is_whitespace(c))
                                break;
                }
        }

        outToken->tokenKind = tokenKind;
        outToken->length = ctx->readPos - parseStart;
        outToken->leadingWhiteChars = leadingWhiteChars;
}

void blunt_begin_lex(struct Blunt_ReadCtx *ctx, struct Textrope *rope, int startPosition)
{
        ctx->rope = rope;
        ctx->readPos = startPosition;
        ctx->bufferStart = 0;
        ctx->bufferLength = 0;
}

void blunt_end_lex(struct Blunt_ReadCtx *ctx)
{
        UNUSED(ctx);
}
