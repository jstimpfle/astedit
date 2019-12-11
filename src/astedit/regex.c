#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/logging.h>
#include <string.h>

enum {
        REGEXNODE_ANY_CHARACTER,
        REGEXNODE_SPECIFIC_CHARACTER,
        REGEXNODE_EMPTY,  // "empty transition"
        REGEXNODE_SPLIT,
};

enum {
        QUANTIFIER_OPTIONAL,
        QUANTIFIER_ONE_OR_MORE,
        QUANTIFIER_NONE_OR_MORE,
};

struct AnyCharacterNode {
        char dummy;
};

struct SpecificCharacterNode {
        int character;
};

struct EmptyRegexNode {
        char dummy;
};

struct SplitRegexNode {
        struct RegexNode *other;
};

struct RegexNode {
        struct RegexNode *next;
        int nodeKind;
        union {
                struct AnyCharacterNode tAnyCharacter;
                struct SpecificCharacterNode tSpecificCharacter;
                struct EmptyRegexNode tEmpty;
                struct SplitRegexNode tSplit;
        } data;
};

struct RegexReadGroupCtx {
        /* a REGEXNODE_EMPTY node that allows us to add new alternatives */
        struct RegexNode *entryNode;
        /* a pre-allocated exit node. This will be used later for the first
         * element following the group, so it's not yet decided what kind
         * of node this is. */
        struct RegexNode *exitNode;
};

struct RegexReadCtx {
        const char *pattern;
        int patternLength;
        int currentCharIndex;

        struct RegexReadGroupCtx *groupStack;
        int groupStackSize;

        struct RegexNode *lastNode;
};

static void _fatal_regex_parse_error_fv(struct LogInfo logInfo,
                                        struct RegexReadCtx *ctx,
                                        const char *fmt, va_list ap)
{
        _log_begin(logInfo);
        log_writef("ERROR In pattern at char #%d: ", ctx->currentCharIndex);
        log_writefv(fmt, ap);
        log_end();
        // should we tag the parse as failed?
}

static void _fatal_regex_parse_error(struct LogInfo logInfo,
                                     struct RegexReadCtx *ctx,
                                     const char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        _fatal_regex_parse_error_fv(logInfo, ctx, fmt, ap);
        va_end(ap);
}

#define fatal_regex_parse_error_fv(ctx, fmt, ap) _fatal_regex_parse_error_fv(MAKE_LOGINFO(), (ctx), (fmt), (ap))
#define fatal_regex_parse_error(ctx, fmt, ...) _fatal_regex_parse_error(MAKE_LOGINFO(), (ctx), (fmt), ##__VA_ARGS__)

static struct RegexNode *alloc_new_node(struct RegexReadCtx *ctx)
{
        UNUSED(ctx);
        struct RegexNode *out;
        ALLOC_MEMORY(&out, 1);
        out->next = NULL;
        return out;
}

static void append_node(struct RegexReadCtx *ctx, struct RegexNode *node)
{
        ENSURE(node->next == NULL);
        ENSURE(ctx->lastNode->next == NULL);
        ctx->lastNode->next = node;
        ctx->lastNode = node;
}

static int next_pattern_char(struct RegexReadCtx *ctx)
{
        if (ctx->currentCharIndex == ctx->patternLength)
                return -1;  // EOF
        return ctx->pattern[ctx->currentCharIndex];
}

static void consume_pattern_char(struct RegexReadCtx *ctx)
{
        ENSURE(ctx->currentCharIndex < ctx->patternLength);
        ctx->currentCharIndex ++;
}

static int finish_regex(struct RegexReadCtx *ctx)
{
        if (ctx->groupStackSize != 1) {
                fatal_regex_parse_error(ctx,
                        "Unexpected end of pattern: unclosed groups remain");
                return 0;
        }
        // TODO: is there more?
        return 1;
}

static void finish_current_alternative(struct RegexReadCtx *ctx)
{
        ENSURE(ctx->groupStackSize > 0);
        struct RegexNode *entryNode = ctx->groupStack[ctx->groupStackSize-1].entryNode;
        struct RegexNode *exitNode = ctx->groupStack[ctx->groupStackSize-1].exitNode;
        append_node(ctx, exitNode);  // to finish current group, append exitNode
        ctx->lastNode = entryNode;
}

static void start_new_alternative(struct RegexReadCtx *ctx)
{
        /* We always expect to have a group, since the code should pop the last
        code only at the end of the pattern. */
        ENSURE(ctx->groupStackSize > 0);
        finish_current_alternative(ctx);
        // insert a split
        ENSURE(ctx->groupStackSize > 0);
        struct RegexNode *entryNode = ctx->groupStack[ctx->groupStackSize-1].entryNode;
        ENSURE(entryNode == ctx->lastNode);
        struct RegexNode *splitNode = alloc_new_node(ctx);
        splitNode->nodeKind = REGEXNODE_SPLIT;
        splitNode->data.tSplit.other = entryNode->next;
        entryNode->next = splitNode;
        ctx->lastNode = splitNode;
}

static void push_group(struct RegexReadCtx *ctx)
{
        struct RegexNode *entryNode = alloc_new_node(ctx);
        struct RegexNode *exitNode = alloc_new_node(ctx);

        entryNode->nodeKind = REGEXNODE_EMPTY;
        exitNode->nodeKind = REGEXNODE_EMPTY;

        append_node(ctx, entryNode);

        ENSURE(ctx->groupStackSize > 0);
        int g = ctx->groupStackSize ++;
        REALLOC_MEMORY(&ctx->groupStack, ctx->groupStackSize);
        ctx->groupStack[g].entryNode = entryNode;
        ctx->groupStack[g].exitNode = exitNode;
}

static int pop_group(struct RegexReadCtx *ctx)
{
        // the initial group will be popped using special code in finish_regex()
        if (ctx->groupStackSize <= 1)
                return 0;
        finish_current_alternative(ctx);
        ENSURE(ctx->groupStackSize > 0);
        ctx->lastNode = ctx->groupStack[ctx->groupStackSize - 1].exitNode;
        ctx->groupStackSize --;
        return 1;
}

static int add_quantifier(struct RegexReadCtx *ctx, int quantifierKind)
{
        // TODO: get "current" element and quantify it.
        return 0;  // no "current element"
}

static void setup_readctx(struct RegexReadCtx *ctx, const char *pattern)
{
        ctx->pattern = pattern;
        ctx->patternLength = strlen(pattern);
        ctx->currentCharIndex = 0;

        ctx->groupStack = NULL;
        ctx->groupStackSize = 0;

        ctx->groupStackSize = 1;
        ALLOC_MEMORY(&ctx->groupStack, ctx->groupStackSize);
        ctx->groupStack[0].entryNode = alloc_new_node(ctx);
        ctx->groupStack[0].exitNode = alloc_new_node(ctx);

        ctx->lastNode = ctx->groupStack[0].entryNode;
}

static void teardown_readctx(struct RegexReadCtx *ctx)
{
        log_postf("TODO: find all the RegexNodes and free them");
        ZERO_MEMORY(ctx);
}

#include <ctype.h>//XXX don't use that, it's locale sensitive and just arcane
static int is_normal_character(int c)
{
        return isprint(c); //XXX
}

static void add_character_node(struct RegexReadCtx *ctx, int character)
{
        struct RegexNode *node = alloc_new_node(ctx);
        node->nodeKind = REGEXNODE_SPECIFIC_CHARACTER;
        node->data.tSpecificCharacter.character = character;
        append_node(ctx, node);
}

static void read_pattern(struct RegexReadCtx *ctx)
{
readmore:;
        int c = next_pattern_char(ctx);
        if (c == -1) {
                if (!finish_regex(ctx)) {
                        return;
                }
                return;
        }
        else if (c == '(') {
                consume_pattern_char(ctx);
                push_group(ctx);
        }
        else if (c == ')') {
                consume_pattern_char(ctx);
                if (!pop_group(ctx)) {
                        fatal_regex_parse_error(ctx,
                                "Closing parenthesis ')' encountered but there is no group to close");
                        return;
                }
        }
        else if (c == '|') {
                consume_pattern_char(ctx);
                start_new_alternative(ctx);
        }
        else if (c == '\\') {
                // read escape sequence
                consume_pattern_char(ctx);
                c = next_pattern_char(ctx);
                if (c == '.') {
                        add_character_node(ctx, '.');
                }
                else if (c == -1) {
                        fatal_regex_parse_error(ctx,
                                "Unfinished escape sequence at end of pattern");
                        return;
                }
                else {
                        fatal_regex_parse_error(ctx,
                                "Invalid escape sequence");
                        return;
                }
        }
        else if (c == '.') {
                consume_pattern_char(ctx);
                // TODO
        }
        else if (c == '?' || c == '*' || c == '+') {
                consume_pattern_char(ctx);
                int quantifierKind;
                if (c == '?')
                        quantifierKind = QUANTIFIER_OPTIONAL;
                else if (c == '*')
                        quantifierKind = QUANTIFIER_NONE_OR_MORE;
                else if (c == '+')
                        quantifierKind = QUANTIFIER_ONE_OR_MORE;
                else
                        UNREACHABLE();
                if (!add_quantifier(ctx, quantifierKind)) {
                        fatal_regex_parse_error(ctx,
                                "Can't add a quantifier here");
                        return;
                }
        }
        else if (is_normal_character(c)) {
                consume_pattern_char(ctx);
                add_character_node(ctx, c);
        }
        else {
                fatal_regex_parse_error(ctx,
                                "Invalid character: '%c'", c);
                return;
        }
        goto readmore;
}

static int match_regex(struct RegexNode *entryNode, const char *string, int length)
{
        // TODO
        return 0;
}

int main(int argc, const char **argv)
{
        for (int i = 1; i < argc; i++) {
                const char *pattern = argv[i];
                log_postf("read pattern: /%s/'", pattern);
                //test_match(test_nodes, LENGTH(test_nodes), argv[i], strlen(argv[i]));
                struct RegexReadCtx readCtx;
                struct RegexReadCtx *ctx = &readCtx;
                setup_readctx(ctx, pattern);
                read_pattern(ctx);
                teardown_readctx(ctx);
        }
        return 0;
}
