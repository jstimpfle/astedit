#include <astedit/astedit.h>
#include <astedit/textrope.h>



enum Blunt_TokenKind {
        BLUNT_TOKEN_EOF,
        BLUNT_TOKEN_NAME,
};

struct Blunt_Token {
        // TODO: more data than just token kind!
        enum Blunt_TokenKind tokenKind;
        int length;
        int leadingWhiteChars;
};


struct Blunt_ReadCtx {
        struct Textrope *rope;
        int readCursor;
        char buffer[512];
        int bufferStart;
        int bufferLength;
};

void blunt_begin_lex(struct Blunt_ReadCtx *ctx, struct Textrope *rope);
void blunt_end_lex(struct Blunt_ReadCtx *ctx);

void blunt_lex_token(struct Blunt_ReadCtx *ctx, struct Blunt_Token *outToken);
