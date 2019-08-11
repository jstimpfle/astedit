#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/textrope.h>
#include <blunt/lex.h>


static void fill_buffer(struct Blunt_ReadCtx *ctx)
{
        ENSURE(ctx->bufferStart + ctx->bufferLength <= sizeof ctx->buffer);
        int bytesAvail = textrope_length(ctx->rope) - ctx->readCursor;

        /* move to front. Simpler than ringbuffer */
        if (ctx->bufferLength < 16 /* just a small number */) {
                move_memory(ctx->buffer + ctx->bufferStart, -ctx->bufferStart, ctx->bufferLength);
                ctx->bufferStart = 0;
        }

        int bytesToRead = sizeof ctx->buffer - (ctx->bufferStart + ctx->bufferLength);
        if (bytesToRead > bytesAvail)
                bytesToRead = bytesAvail;
        copy_text_from_textrope(ctx->rope, ctx->readCursor, ctx->buffer + ctx->bufferStart, bytesToRead);
        ctx->bufferLength += bytesToRead;
}

static int has_byte(struct Blunt_ReadCtx *ctx)
{
        if (ctx->bufferLength > 0)
                fill_buffer(ctx);
        return ctx->bufferLength > 0;
}

static int look_byte(struct Blunt_ReadCtx *ctx)
{
        ENSURE(has_byte(ctx));
        int c = ctx->buffer[ctx->bufferStart];
        return c;
}

static void consume_byte(struct Blunt_ReadCtx *ctx)
{
        ENSURE(ctx->bufferLength > 0);
        ctx->bufferStart++;
        ctx->bufferLength--;
}

static int is_whitespace(int c)
{
        return (unsigned) c <= 32;
}

/* TODO: return more information than just a token kind. */
void blunt_lex_token(struct Blunt_ReadCtx *ctx, struct Blunt_Token *outToken)
{
        enum Blunt_TokenKind tokenKind;
        int c;

        int parseStart = ctx->readCursor;
        int leadingWhiteChars = 0;

        for (;;) {
                if (!has_byte(ctx)) {
                        tokenKind = BLUNT_TOKEN_EOF;
                        goto out;
                }
                c = look_byte(ctx);
                if (!is_whitespace(c))
                        break;
                consume_byte(ctx);
        }

        leadingWhiteChars = ctx->readCursor - parseStart;

        /* consume first character. */
        consume_byte(ctx);

        /* only names, for now. */
        tokenKind = BLUNT_TOKEN_NAME;

        for (;;) {
                if (!has_byte(ctx))
                        break;
                c = look_byte(ctx);
                while (is_whitespace(c))
                        break;
                consume_byte(ctx);
        }

out:
        outToken->tokenKind = tokenKind;
        outToken->tokenKind = BLUNT_TOKEN_NAME;
        outToken->length = ctx->readCursor - parseStart;
        outToken->leadingWhiteChars = leadingWhiteChars;
}

void blunt_begin_lex(struct Blunt_ReadCtx *ctx, struct Textrope *rope)
{
        ctx->rope = rope;
        ctx->readCursor = 0;
        ctx->bufferStart = 0;
        ctx->bufferLength = 0;
}

void blunt_end_lex(struct Blunt_ReadCtx *ctx)
{
        UNUSED(ctx);
}
