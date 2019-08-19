#if 0
#include <astedit/astedit.h>
#include <blunt/lex.h>

enum Blunt_PrecedenceKind {
        BLUNT_PRECEDENCE_PAREN,
        BLUNT_PRECEDENCE_MUL,
        BLUNT_PRECEDENCE_ADD,
        BLUNT_PRECEDENCE_COMPARE,
        BLUNT_PRECEDENCE_ASSIGN,
};

enum Blunt_NodeKind {
        BLUNT_NODE_LITERAL_EXPRESSION,
};

enum Blunt_ExprKind {
        BLUNT_EXPR_INVALID,
        BLUNT_EXPR_LITERAL_INTEGER,
};


struct Blunt_Expr {
        enum Blunt_ExprKind exprKind;
};

struct Blunt_ExprInvalid {
        struct Blunt_Expr expr;
        /* nothing else */
};

struct Blunt_ExprLiteralInteger {
        int64_t integerValue;  // for now. Later, larger value range.
};

struct Blunt_ExprBinop {
        /* binop kind is contained in node kind */
        struct Blunt_Expr *lhs;
        struct Blunt_Expr *rhs;
};



static struct Blunt_Token savedToken;
static int haveSavedToken;



static void look_token(struct Blunt_ReadCtx *ctx, struct Blunt_Token *token)
{
        if (!haveSavedToken) {
                lex_blunt_token(ctx, &savedToken);
                haveSavedToken = 1;
        }
        *token = savedToken;
}

static void consume_token(void)
{
        ENSURE(haveSavedToken);
        ENSURE(savedToken.tokenKind != BLUNT_TOKEN_EOF);
        haveSavedToken = 0;
}

static int is_operator(struct Blunt_Token *token)
{
        switch (token->tokenKind) {
        case BLUNT_TOKEN_INTEGER:
                return 1;
        default:
                return 0;
        }
}

struct Blunt_ExprNode *allocate_blunt_expr(enum Blunt_NodeKind nodeKind)
{
        return NULL;  // TODO
}


struct Blunt_ExprLiteralInteger *make_blunt_expr_invalid(void)
{
        struct Blunt_ExprInvalid *node = allocate_blunt_node(BLUNT_EXPR_INVALID);
        init_blunt_expr(node);
}

struct Blunt_ExprLiteral *make_blunt_expr_literal(enum Blunt_TokenKind tokenKind)
{
        struct Blunt_ExprLiteral *node = make_blunt_expr(BLUNT_NODE_LITERAL_EXPRESSION);
        return node;
}

struct Blunt_ExprLiteralInteger *make_blunt_expr_literal_integer(int64_t integerValue)
{
        struct Blunt_ExprLiteralInteger *node = make_blunt_expr_literal(BLUNT_EXPR_LITERAL_INTEGER);
        node->integerValue = integerValue;
        return node;
}

struct Blunt_ExprBinop *make_blunt_expr_binop(int binopKind)
{
        struct Blunt_ExprBinop *node = make_blunt_expr();
}




struct Blunt_Expr *parse_blunt_expr(struct Blunt_ReadCtx *ctx, enum Blunt_PrecedenceKind precedenceKind)
{
        struct Blunt_Token token;
        struct Blunt_Expr *expr;
        
        look_token(ctx, &token);
        consume_token();
        
        if (token.tokenKind == BLUNT_TOKEN_INTEGER) {
                expr = make_blunt_expr_literal_integer(42);                
        }
        else if (token.tokenKind == BLUNT_TOKEN_LPAREN) {
                struct Blunt_Token lparen = token;
                consume_token();
                expr = parse_blunt_expr(ctx, BLUNT_PRECEDENCE_PAREN);
                look_token(ctx, &token);
                if (token.tokenKind != BLUNT_TOKEN_RPAREN) {
                        /*TODO: recover*/
                }
                struct Blunt_Token rparen = token;
                /* TODO: "parenthesized expression" or just operator expression with proper bounds? */
        }
        else {
                // TODO find recovery point
                return make_blunt_expr_invalid();
        }

        for (;;) {
                look_token(ctx, &token);

                if (!is_operator_with_matching_precedence(&token), precedenceKind)
                        break;

                consume_token();
                struct Blunt_Expr *lhs = expr;
                struct Blunt_Expr *rhs = parse_blunt_expr(ctx, precedenceKind);
                expr = make_blunt_expr_binop(&token, lhs, rhs);
        }

        return expr;
}














/******************************/


enum Blunt_ParseState {
        BLUNT_PARSE_EXPR_WITH_PRECEDENCE,
        BLUNT_PARSE_POSSIBLE_BINOP_AFTER_EXPR,
};

struct Blunt_ParseCtx {
        enum Blunt_ParseState parseState;
        union {
                struct {
                        enum Blunt_PrecedenceKind precedenceKind;
                } tParseExprWithPrecedence;

                struct {
                        enum Blunt_PrecedenceKind precedenceKind;
                        struct Blunt_Node *node;
                } tParsePossibleBinopAfterExpr;
        };
};

void parse_expr_with_precedence(struct Blunt_ParseCtx *ctx)
{
        struct Blunt_Token token;
        look_token(ctx, &token);
        switch (token.tokenKind) {
        case BLUNT_TOKEN_LPAREN:
                consume_token(ctx);
                NEXT(BLUNT_PARSE_EXPR_WITH_PRECEDENCE
                break;
        case BLUNT_TOKEN_INTEGER:
        case BLUNT_TOKEN_EOF:

        }

        if (ctx->tParseExprWithPrecedence.precedenceKind) {
                
        }
                
}

void parse_possible_binop_after_blunt_expression(struct Blunt_ParseCtx *ctx)
{
        operator_with_lower_precedence(ctx->
}
#endif