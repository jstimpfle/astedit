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

void find_start_of_next_token(struct Blunt_ReadCtx *ctx)
{
        for (;;) {
                int c = look_byte(ctx);
                if (c == -1)
                        break;
                if (!is_whitespace(c))
                        break;
                consume_byte(ctx);
        }
}

struct OpInfo1 {
        int character;
        int tokenKind;
};

struct OpInfo2 {
        int character1;
        int character2;
        int tokenKind1;
        int tokenKind2;
};

static const struct OpInfo1 opInfo1[] = {
        { '*', BLUNT_TOKEN_STAR },
        /* We need to do this using special code to support comments.
         { '/', BLUNT_TOKEN_SLASH },
         */
        { '!', BLUNT_TOKEN_LOGICALAND },
        { '^', BLUNT_TOKEN_BITWISEXOR },
};

static const struct OpInfo2 opInfo2[] = {
        { '+', '+', BLUNT_TOKEN_PLUS, BLUNT_TOKEN_DOUBLEPLUS },
        { '-', '-', BLUNT_TOKEN_MINUS, BLUNT_TOKEN_MINUS },
        { '<', '<', BLUNT_TOKEN_LESSTHAN, BLUNT_TOKEN_SHIFTLEFT },
        { '>', '>', BLUNT_TOKEN_GREATERTHAN, BLUNT_TOKEN_SHIFTRIGHT },
        { '&', '&', BLUNT_TOKEN_BITWISEAND, BLUNT_TOKEN_LOGICALAND },
        { '|', '|', BLUNT_TOKEN_BITWISEOR, BLUNT_TOKEN_LOGICALOR },
};

#if 0
static const char *const bluntTokenKindString[NUM_BLUNT_TOKEN_KINDS] = {
#define MAKE(x) [x] = #x
        MAKE( BLUNT_TOKEN_EOF ),
        MAKE( BLUNT_TOKEN_NAME ),
        MAKE( BLUNT_TOKEN_INTEGER ),
        MAKE( BLUNT_TOKEN_STRING ),
        MAKE( BLUNT_TOKEN_LPAREN ),
        MAKE( BLUNT_TOKEN_RPAREN ),
        MAKE( BLUNT_TOKEN_COMMENT ),
        MAKE( BLUNT_TOKEN_PLUS ),
        MAKE( BLUNT_TOKEN_MINUS ),
        MAKE( BLUNT_TOKEN_STAR ),
        MAKE( BLUNT_TOKEN_SLASH ),
        MAKE( BLUNT_TOKEN_DOUBLEPLUS ),
        MAKE( BLUNT_TOKEN_DOUBLEMINUS ),
        MAKE( BLUNT_TOKEN_GREATERTHAN ),
        MAKE( BLUNT_TOKEN_LESSTHAN ),
        MAKE( BLUNT_TOKEN_SHIFTLEFT ),
        MAKE( BLUNT_TOKEN_SHIFTRIGHT ),
        MAKE( BLUNT_TOKEN_LOGICALNOT ),
        MAKE( BLUNT_TOKEN_LOGICALOR ),
        MAKE( BLUNT_TOKEN_LOGICALAND ),
        MAKE( BLUNT_TOKEN_BITWISEXOR ),
        MAKE( BLUNT_TOKEN_BITWISEAND ),
        MAKE( BLUNT_TOKEN_BITWISEOR ),
        MAKE( BLUNT_TOKEN_JUNK ),
#undef MAKE
};
#endif

/* TODO: return more information than just a token kind. */
void lex_blunt_token(struct Blunt_ReadCtx *ctx, struct Blunt_Token *outToken)
{
        FILEPOS parseStart = ctx->readPos;
        FILEPOS leadingWhiteChars = 0;

        find_start_of_next_token(ctx);
        leadingWhiteChars = filepos_sub(ctx->readPos, parseStart);

        enum Blunt_TokenKind tokenKind = -1;
        int c = look_byte(ctx);  // we already called find_start_of_next_token(). Calling look_byte() again is not very nice
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

        if (tokenKind == -1) {
                for (int i = 0; i < LENGTH(opInfo1); i++) {
                        if (c == opInfo1[i].character) {
                                consume_byte(ctx);
                                tokenKind = opInfo1[i].tokenKind;
                                break;
                        }
                }
        }

        if (tokenKind == -1) {
                for (int i = 0; i < LENGTH(opInfo2); i++) {
                        if (c == opInfo2[i].character1) {
                                consume_byte(ctx);
                                c = look_byte(ctx);
                                if (c == opInfo2[i].character2) {
                                        consume_byte(ctx);
                                        tokenKind = opInfo2[i].tokenKind2;
                                }
                                else {
                                        tokenKind = opInfo2[i].tokenKind1;
                                }
                                break;
                        }
                }
        }

        if (tokenKind == -1) {
                if (c == '/') {
                        consume_byte(ctx);
                        c = look_byte(ctx);
                        if (c == '*') {
                                consume_byte(ctx);
                                for (;;) {
                                        c = look_byte(ctx);
                                        if (c == -1)
                                                break; // how to handle EOF here?
                                        consume_byte(ctx);
                                        if (c == '*') {
                                                c = look_byte(ctx);
                                                if (c == '/') {
                                                        consume_byte(ctx);
                                                        break;
                                                }
                                        }
                                }
                                tokenKind = BLUNT_TOKEN_COMMENT;
                        }
                        else if (c == '/') {
                                consume_byte(ctx);
                                for (;;) {
                                        c = look_byte(ctx);
                                        if (c == -1)
                                                break;
                                        consume_byte(ctx);
                                        if (c == '\n')
                                                break;
                                }
                                tokenKind = BLUNT_TOKEN_COMMENT;
                        }
                        else {
                                tokenKind = BLUNT_TOKEN_SLASH;
                        }
                }
        }

        if (tokenKind == -1) {
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

        ENSURE(ctx->readPos <= textrope_length(ctx->rope));
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
