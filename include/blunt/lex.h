#include <astedit/astedit.h>
#include <astedit/textrope.h>



enum Blunt_TokenKind {
        BLUNT_TOKEN_EOF,
        BLUNT_TOKEN_NAME,
        BLUNT_TOKEN_INTEGER,
        BLUNT_TOKEN_STRING,
        // operators
        BLUNT_TOKEN_PLUS,
        BLUNT_TOKEN_MINUS,
        BLUNT_TOKEN_STAR,
        BLUNT_TOKEN_SLASH,
        //
        BLUNT_TOKEN_JUNK,
};

enum {
        FIRST_BLUNT_TOKEN_OPERATOR = BLUNT_TOKEN_PLUS,
        LAST_BLUNT_TOKEN_OPERATOR = BLUNT_TOKEN_SLASH,
};

struct Blunt_Token {
        // TODO: more data than just token kind!
        enum Blunt_TokenKind tokenKind;
        int length;
        int leadingWhiteChars;
};


struct Blunt_ReadCtx {
        struct Textrope *rope;
        int readPos;
        char buffer[512];
        int bufferStart;
        int bufferLength;
};

void blunt_begin_lex(struct Blunt_ReadCtx *ctx, struct Textrope *rope, int startPos);
void blunt_end_lex(struct Blunt_ReadCtx *ctx);

void blunt_lex_token(struct Blunt_ReadCtx *ctx, struct Blunt_Token *outToken);
