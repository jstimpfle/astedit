#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/textrope.h>
#include <astedit/logging.h>
#include <astedit/utf8.h>
#include <blunt/lex.h>


static void fill_buffer(struct Blunt_ReadCtx *ctx)
{
        ENSURE(ctx->bufferStart + ctx->bufferLength <= sizeof ctx->buffer);

        /* move to front. Simpler than ringbuffer */
        if (ctx->bufferLength < 16 /* arbitrary, small number */) {
                move_memory(ctx->buffer + ctx->bufferStart, -ctx->bufferStart, ctx->bufferLength);
                ctx->bufferStart = 0;
        }

        int bytesToRead = min_filepos_as_int(
                sizeof ctx->buffer - (ctx->bufferStart + ctx->bufferLength),
                textrope_length(ctx->rope) - ctx->readPos);
        /* note that "readPos" is ignoring what's already in the buffer.
        That's why we add bufferLength here, and also why we don't advance
        readPos after copy_text_from_textrope() */
        copy_text_from_textrope(ctx->rope, ctx->readPos + ctx->bufferLength,
                ctx->buffer + ctx->bufferStart, bytesToRead);
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
        int c = (unsigned char) ctx->buffer[ctx->bufferStart];
        return c;
}

static void consume_byte(struct Blunt_ReadCtx *ctx)
{
        ENSURE(ctx->bufferLength > 0);
        ctx->readPos++;
        ctx->bufferStart++;
        ctx->bufferLength--;
}

static int is_alpha(int c)
{
        return ('a' <= c && c <= 'z')
                || ('A' <= c && c <= 'Z');
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
void lex_blunt_token(struct Blunt_ReadCtx *ctx, struct Blunt_Token *outToken)
{
        enum Blunt_TokenKind tokenKind;
        int c;

        FILEPOS parseStart = ctx->readPos;
        FILEPOS leadingWhiteChars = 0;

        for (;;) {
                c = look_byte(ctx);
                if (c == -1)
                        break;
                if (!is_whitespace(c))
                        break;
                consume_byte(ctx);
        }
        leadingWhiteChars = filepos_sub(ctx->readPos, parseStart);

        if (c == -1) {
                tokenKind = BLUNT_TOKEN_EOF;
        }
        else if (is_alpha(c) || c == '_') {
                tokenKind = BLUNT_TOKEN_NAME;
                for (;;) {
                        consume_byte(ctx);
                        c = look_byte(ctx);
                        if (c == -1)
                                break;
                        if (!(is_alpha(c) || is_digit(c) || c == '_'))
                                break;
                }
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
        else if (c == '"') {
                tokenKind = BLUNT_TOKEN_STRING;
                consume_byte(ctx);
                // TODO: store the actual string
                for (;;) {
                        c = look_byte(ctx);
                        if (c == -1)
                                break; // TODO: error!
                        if (c < 32)
                                break; // TODO: error!
                        if (c == '"') {
                                consume_byte(ctx);
                                break;
                        }
                        else if (c == '\\') {
                                consume_byte(ctx);
                                c = look_byte(ctx);
                                if (c == -1)
                                        break;  // TODO: error!
                                /* TODO more escaping */
                        }
                        consume_byte(ctx);
                }
        }
#define OP(x, y) else if (c == x) { tokenKind = y; consume_byte(ctx); }
        OP('+', BLUNT_TOKEN_PLUS)
                OP('-', BLUNT_TOKEN_MINUS)
                OP('*', BLUNT_TOKEN_STAR)
                OP('/', BLUNT_TOKEN_SLASH)
        else {
                tokenKind = BLUNT_TOKEN_JUNK;
                for (;;) {
                        consume_byte(ctx);
                        c = look_byte(ctx);
                        if (c == -1)
                                break;
                        if (is_utf8_leader_byte(c))
                                break;
                }
                // how much to consume?
        }

        outToken->tokenKind = tokenKind;
        outToken->length = filepos_sub(ctx->readPos, parseStart);
        outToken->leadingWhiteChars = leadingWhiteChars;
}

void begin_lexing_blunt_tokens(struct Blunt_ReadCtx *ctx, struct Textrope *rope, FILEPOS startPosition)
{
        ctx->rope = rope;
        ctx->readPos = startPosition;
        ctx->bufferStart = 0;
        ctx->bufferLength = 0;
}

void end_lexing_blunt_tokens(struct Blunt_ReadCtx *ctx)
{
        UNUSED(ctx);
}