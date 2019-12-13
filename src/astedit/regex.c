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

struct SpecificCharacterNode {
        int character;
};

struct SplitRegexNode {
        int otherIndex;
};

struct RegexNode {
        int nextIndex;
        int nodeKind;
        union {
                struct SpecificCharacterNode tSpecificCharacter;
                struct SplitRegexNode tSplit;
        } data;
};

struct RegexReadGroupCtx {
        /* a REGEXNODE_EMPTY node that allows us to add new alternatives */
        int entryNodeIndex;
        /* a pre-allocated exit node. This will be used later for the first
         * element following the group, so it's not yet decided what kind
         * of node this is. */
        int exitNodeIndex;
};

struct RegexReadCtx {
        int bad;  // bad flag

        const char *pattern;
        int patternLength;
        int currentCharIndex;

        struct RegexReadGroupCtx *groupStack;
        int groupStackSize;

        struct RegexNode *nodes;
        int numNodes;

        int initialNodeIndex;
        int lastNodeIndex;  // current last node, where new nodes get appended
};

static void _fatal_regex_parse_error_fv(struct LogInfo logInfo,
                                        struct RegexReadCtx *ctx,
                                        const char *fmt, va_list ap)
{
        ctx->bad = 1;

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

static int alloc_new_node(struct RegexReadCtx *ctx)
{
        int nodeIndex = ctx->numNodes ++;
        REALLOC_MEMORY(&ctx->nodes, ctx->numNodes);
        ctx->nodes[nodeIndex].nextIndex = -1;
        return nodeIndex;
}

static void append_node(struct RegexReadCtx *ctx, int nodeIndex)
{
        ENSURE(ctx->nodes[nodeIndex].nextIndex == -1);
        ENSURE(ctx->nodes[ctx->lastNodeIndex].nextIndex == -1);
        ctx->nodes[ctx->lastNodeIndex].nextIndex = nodeIndex;
        ctx->lastNodeIndex = nodeIndex;
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
        int entryNodeIndex = ctx->groupStack[ctx->groupStackSize-1].entryNodeIndex;
        int exitNodeIndex = ctx->groupStack[ctx->groupStackSize-1].exitNodeIndex;
        append_node(ctx, exitNodeIndex);  // to finish current group, append exitNode
        ctx->lastNodeIndex = entryNodeIndex;
}

static void start_new_alternative(struct RegexReadCtx *ctx)
{
        /* We always expect to have a group, since the code should pop the last
        code only at the end of the pattern. */
        ENSURE(ctx->groupStackSize > 0);
        finish_current_alternative(ctx);
        // insert a split
        ENSURE(ctx->groupStackSize > 0);
        int entryNodeIndex = ctx->groupStack[ctx->groupStackSize-1].entryNodeIndex;
        ENSURE(entryNodeIndex == ctx->lastNodeIndex);
        int splitNodeIndex = alloc_new_node(ctx);
        ctx->nodes[splitNodeIndex].nodeKind = REGEXNODE_SPLIT;
        ctx->nodes[splitNodeIndex].data.tSplit.otherIndex = ctx->nodes[entryNodeIndex].nextIndex;
        ctx->nodes[entryNodeIndex].nextIndex = splitNodeIndex;
        ctx->lastNodeIndex = splitNodeIndex;
}

static void push_group(struct RegexReadCtx *ctx)
{
        /* This function also needs to work when ctx->groupStackSize == 0,
         * since it is called in setup_readctx(). Otherwise, we can expect
         * to have ctx->groupStackSize > 0. */
        int entryNodeIndex = alloc_new_node(ctx);
        int exitNodeIndex = alloc_new_node(ctx);
        ctx->nodes[entryNodeIndex].nodeKind = REGEXNODE_EMPTY;
        ctx->nodes[exitNodeIndex].nodeKind = REGEXNODE_EMPTY;

        append_node(ctx, entryNodeIndex);

        int g = ctx->groupStackSize ++;
        REALLOC_MEMORY(&ctx->groupStack, ctx->groupStackSize);
        ctx->groupStack[g].entryNodeIndex = entryNodeIndex;
        ctx->groupStack[g].exitNodeIndex = exitNodeIndex;
}

static int pop_group(struct RegexReadCtx *ctx)
{
        // the initial group will be popped using special code in finish_regex()
        if (ctx->groupStackSize <= 1)
                return 0;
        finish_current_alternative(ctx);
        ENSURE(ctx->groupStackSize > 0);
        ctx->lastNodeIndex = ctx->groupStack[ctx->groupStackSize - 1].exitNodeIndex;
        ctx->groupStackSize --;
        return 1;
}

static int add_quantifier(struct RegexReadCtx *ctx, int quantifierKind)
{
        UNUSED(ctx);
        UNUSED(quantifierKind);
        // TODO: get "current" element and quantify it.
        return 0;  // no "current element"
}

static void setup_readctx(struct RegexReadCtx *ctx, const char *pattern)
{
        ctx->bad = 0;

        ctx->pattern = pattern;
        ctx->patternLength = (int) strlen(pattern);
        ctx->currentCharIndex = 0;

        ctx->groupStack = NULL;
        ctx->groupStackSize = 0;

        ctx->nodes = NULL;
        ctx->numNodes = 0;
        ctx->lastNodeIndex = -1;

        int initialNodeIndex = alloc_new_node(ctx);
        ctx->nodes[initialNodeIndex].nodeKind = REGEXNODE_EMPTY;
        ctx->initialNodeIndex = initialNodeIndex;
        ctx->lastNodeIndex = initialNodeIndex;

        // add a base group that is always there
        push_group(ctx);
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
        int nodeIndex = alloc_new_node(ctx);
        ctx->nodes[nodeIndex].nodeKind = REGEXNODE_SPECIFIC_CHARACTER;
        ctx->nodes[nodeIndex].data.tSpecificCharacter.character = character;
        append_node(ctx, nodeIndex);
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


struct MatchCtx {
        struct RegexNode *nodes;
        int numNodes;

        int *isActive;
        int *nextIsActive;
};

static void set_active(struct MatchCtx *ctx, int nodeIndex)
{
        if (ctx->nodes[nodeIndex].nodeKind == REGEXNODE_SPLIT) {
                set_active(ctx, ctx->nodes[nodeIndex].nextIndex);
                set_active(ctx, ctx->nodes[nodeIndex].data.tSplit.otherIndex);
        }
        else if (ctx->nodes[nodeIndex].nodeKind == REGEXNODE_EMPTY) {
                set_active(ctx, ctx->nodes[nodeIndex].nextIndex);
        }
        else {
                ctx->nextIsActive[nodeIndex] = 1;
        }
}

static void set_successors_active(struct MatchCtx *ctx, int nodeIndex)
{
        ENSURE(0 <= nodeIndex && nodeIndex < ctx->numNodes);
        ENSURE(ctx->nodes[nodeIndex].nodeKind != REGEXNODE_SPLIT);
        ENSURE(ctx->nodes[nodeIndex].nodeKind != REGEXNODE_EMPTY);

        int nextNodeIndex = ctx->nodes[nodeIndex].nextIndex;
        set_active(ctx, nextNodeIndex);
}

static void prepare_matching_for_next_char(struct MatchCtx *ctx)
{
        int *tmp = ctx->isActive;
        ctx->isActive = ctx->nextIsActive;
        ctx->nextIsActive = tmp;
        for (int i = 0; i < ctx->numNodes; i++)
                ctx->nextIsActive[i] = 0;
}

static void setup_matchctx_from_readctx(struct MatchCtx *matchCtx,
                                        struct RegexReadCtx *readCtx)
{
        matchCtx->nodes = readCtx->nodes;
        matchCtx->numNodes = readCtx->numNodes;
        ALLOC_MEMORY(&matchCtx->isActive, matchCtx->numNodes);
        ALLOC_MEMORY(&matchCtx->nextIsActive, matchCtx->numNodes);
        for (int i = 0; i < matchCtx->numNodes; i++)
                matchCtx->nextIsActive[i] = 0;
        ENSURE(matchCtx->nodes[readCtx->initialNodeIndex].nodeKind == REGEXNODE_EMPTY);
        set_active(matchCtx, matchCtx->nodes[readCtx->initialNodeIndex].nextIndex);
        prepare_matching_for_next_char(matchCtx);
}

static void release_matchctx_after_setup_from_readctx(struct MatchCtx *matchCtx)
{
        FREE_MEMORY(&matchCtx->isActive);
        FREE_MEMORY(&matchCtx->nextIsActive);
}

static int match_regex(struct RegexReadCtx *readCtx, const char *string, int length)
{
        struct MatchCtx matchCtx;
        struct MatchCtx *const ctx = &matchCtx;
        setup_matchctx_from_readctx(&matchCtx, readCtx);

        for (int i = 0;
             i < length;
             i++, prepare_matching_for_next_char(ctx))
        {
                int c = string[i];
                log_postf("now at character '%c'", c);
                for (int nodeIndex = 0; nodeIndex < ctx->numNodes; nodeIndex++) {
                        if (!ctx->isActive[nodeIndex])
                                continue;
                        struct RegexNode *node = &ctx->nodes[nodeIndex];
                        switch (node->nodeKind) {
                        case REGEXNODE_SPECIFIC_CHARACTER:
                                if (c == node->data.tSpecificCharacter.character) {
                                        log_postf("match specific character.");
                                        set_successors_active(ctx, nodeIndex);
                                }
                                break;
                        case REGEXNODE_ANY_CHARACTER:
                                log_postf("match any character.");
                                set_successors_active(ctx, nodeIndex);
                                break;
                        }
                }
        }

        release_matchctx_after_setup_from_readctx(ctx);//XXX

        return 1;//XXX
}

/*
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
                if (ctx->bad)
                        log_postf("abort due to pattern read error.");
                else
                        match_regex(ctx, "foobar", sizeof "foobar" - 1);
                teardown_readctx(ctx);
        }
        return 0;
}
*/