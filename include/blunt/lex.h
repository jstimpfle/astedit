#include <astedit/astedit.h>
#include <astedit/filepositions.h>
#include <astedit/textrope.h>


enum Blunt_TokenKind {
        BLUNT_TOKEN_EOF,
        BLUNT_TOKEN_NAME,
        BLUNT_TOKEN_INTEGER,
        BLUNT_TOKEN_STRING,
        BLUNT_TOKEN_LPAREN,
        BLUNT_TOKEN_RPAREN,
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
        // currently these are large integers like file position values,
        // but if we have many tokens we might want to limit the
        // valid size of tokens to a sane value to save memory.
        FILEPOS length;
        FILEPOS leadingWhiteChars;
};


struct Blunt_ReadCtx {
        struct Textrope *rope;
        FILEPOS readPos;
        char buffer[512];
        int bufferStart;
        int bufferLength;
};

void find_start_of_next_token(struct Blunt_ReadCtx *ctx);
void begin_lexing_blunt_tokens(struct Blunt_ReadCtx *ctx, struct Textrope *rope, FILEPOS startPos);
void end_lexing_blunt_tokens(struct Blunt_ReadCtx *ctx);

void lex_blunt_token(struct Blunt_ReadCtx *ctx, struct Blunt_Token *outToken);
