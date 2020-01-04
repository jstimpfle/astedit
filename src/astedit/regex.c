#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/logging.h>
#include <astedit/regex.h>
#include <string.h>

struct RegexReadGroupCtx {
        int groupKind;  // REGEX_GROUP_??
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
        /* first "implementation node" corresponding to last syntactical element
        (so we can apply quantifiers to it), or -1 if no last element (at the start
        of a group or alternative) */
        int startIndexOfLastElement;
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
        return (int) (unsigned char) ctx->pattern[ctx->currentCharIndex];
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
        int otherIndex = ctx->nodes[entryNodeIndex].nextIndex;
        ctx->nodes[entryNodeIndex].nextIndex = splitNodeIndex;
        ctx->nodes[splitNodeIndex].nodeKind = REGEXNODE_SPLIT;
        ctx->nodes[splitNodeIndex].data.tSplit.otherIndex = otherIndex;
        ctx->lastNodeIndex = splitNodeIndex;
        ctx->startIndexOfLastElement = -1;
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
        ctx->startIndexOfLastElement = -1;

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
        /* if the following character is a '*' we will need to know the index of
         * the first node. */
        ctx->startIndexOfLastElement = ctx->groupStack[ctx->groupStackSize - 1].entryNodeIndex;
        ctx->lastNodeIndex = ctx->groupStack[ctx->groupStackSize - 1].exitNodeIndex;
        ctx->groupStackSize --;
        return 1;
}

static int add_quantifier(struct RegexReadCtx *ctx, int quantifierKind)
{
        UNUSED(ctx);
        UNUSED(quantifierKind);
        if (ctx->startIndexOfLastElement == -1)
                return 0;
        if (quantifierKind == QUANTIFIER_NONE_OR_MORE
            || quantifierKind == QUANTIFIER_ONE_OR_MORE) {
                /* insert an optional jump back to the start of the last
                 * element. */
                int splitNodeIndex = alloc_new_node(ctx);
                ctx->nodes[splitNodeIndex].nextIndex = -1;
                ctx->nodes[splitNodeIndex].nodeKind = REGEXNODE_SPLIT;
                ctx->nodes[splitNodeIndex].data.tSplit.otherIndex = ctx->startIndexOfLastElement;
                append_node(ctx, splitNodeIndex);
        }

        if (quantifierKind == QUANTIFIER_OPTIONAL
            || quantifierKind == QUANTIFIER_NONE_OR_MORE) {
                /* an empty node that is (with the current approach) needed
                 * to skip over the last element. */
                int afterIndex = alloc_new_node(ctx);
                ctx->nodes[afterIndex].nextIndex = -1;
                ctx->nodes[afterIndex].nodeKind = REGEXNODE_EMPTY;
                append_node(ctx, afterIndex);
                /* Since we don't know all the places that lead to the start
                 * node of the last element, we can't update them to add a split
                 * there. Unless we improve the architecture here and introdue a
                 * real AST for the parsed regex and so on, - the only thing we
                 * can do is make a copy of that node, and change the old node
                 * into a split node. */
                int oldIndex = ctx->startIndexOfLastElement;
                int copyIndex = alloc_new_node(ctx);
                ctx->nodes[copyIndex] = ctx->nodes[oldIndex];
                ctx->nodes[oldIndex].nextIndex = afterIndex;
                ctx->nodes[oldIndex].nodeKind = REGEXNODE_SPLIT;
                ctx->nodes[oldIndex].data.tSplit.otherIndex = copyIndex;
                /* It's all pretty shaky. Hope the code is correct! */
        }
        ctx->startIndexOfLastElement = -1; // makes sense?
        return 1;
}

static void add_character_node(struct RegexReadCtx *ctx, int character)
{
        int nodeIndex = alloc_new_node(ctx);
        ctx->nodes[nodeIndex].nodeKind = REGEXNODE_SPECIFIC_CHARACTER;
        ctx->nodes[nodeIndex].data.tSpecificCharacter.character = character;
        append_node(ctx, nodeIndex);
        ctx->startIndexOfLastElement = nodeIndex;
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
                int nodeIndex = alloc_new_node(ctx);
                ctx->nodes[nodeIndex].nodeKind = REGEXNODE_ANY_CHARACTER;
                ctx->nodes[nodeIndex].data.tSpecificCharacter.character = c;
                append_node(ctx, nodeIndex);
                ctx->startIndexOfLastElement = nodeIndex;
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
        else if (c >= 32) {
                consume_pattern_char(ctx);
                add_character_node(ctx, c);
        }
        else {
                fatal_regex_parse_error(ctx,
                                "Invalid character: '%c' (%d)", c, c);
                return;
        }
        goto readmore;
}

static void setup_readctx(struct RegexReadCtx *ctx, const char *pattern, int length)
{
        ctx->bad = 0;

        ctx->pattern = pattern;
        ctx->patternLength = length;
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
        ctx->startIndexOfLastElement = -1;  // no last element. technically not required since we push_group() below

        // add a base group that is always there
        push_group(ctx);
}

static void teardown_readctx(struct RegexReadCtx *ctx)
{
        FREE_MEMORY(&ctx->nodes);
        ZERO_MEMORY(ctx);
}

static void extract_regex_from_readctx(struct Regex *regex, struct RegexReadCtx *ctx)
{
        regex->nodes = ctx->nodes;
        regex->numNodes = ctx->numNodes;
        regex->initialNodeIndex = ctx->initialNodeIndex;

        ctx->nodes = NULL;
        ctx->numNodes = 0;
        ctx->initialNodeIndex = 0;
}

int compile_regex_from_pattern(struct Regex *regex, const char *pattern, int length)
{
        struct RegexReadCtx ctx;
        setup_readctx(&ctx, pattern, length);
        read_pattern(&ctx);
        int good = !ctx.bad;
        if (good)
                extract_regex_from_readctx(regex, &ctx);
        teardown_readctx(&ctx);
        return good;
}

void setup_regex(struct Regex *regex)
{
        ZERO_MEMORY(&regex);
}

void teardown_regex(struct Regex *regex)
{
        FREE_MEMORY(&regex->nodes);
        ZERO_MEMORY(&regex);
}


static void set_active(struct MatchCtx *ctx, int nodeIndex, FILEPOS earliestMatchPos)
{
        if (nodeIndex == -1) {
                ctx->haveMatch = 1;  // TODO: better way to report match
                ctx->matchStartPos = earliestMatchPos;
                return;
        }
        struct NodeMatchState *state = &ctx->nextMatchState[nodeIndex];
        if (state->isActive && state->earliestMatchPos <= earliestMatchPos)
                return;
        ENSURE(0 <= nodeIndex && nodeIndex < ctx->numNodes);
        state->isActive = 1;
        state->earliestMatchPos = earliestMatchPos;
        if (ctx->nodes[nodeIndex].nodeKind == REGEXNODE_SPLIT) {
                set_active(ctx, ctx->nodes[nodeIndex].nextIndex, earliestMatchPos);
                set_active(ctx, ctx->nodes[nodeIndex].data.tSplit.otherIndex, earliestMatchPos);
        }
        else if (ctx->nodes[nodeIndex].nodeKind == REGEXNODE_EMPTY) {
                set_active(ctx, ctx->nodes[nodeIndex].nextIndex, earliestMatchPos);
        }
}

static void set_successors_active(struct MatchCtx *ctx, int nodeIndex)
{
        ENSURE(0 <= nodeIndex && nodeIndex < ctx->numNodes);
        ENSURE(ctx->nodes[nodeIndex].nodeKind != REGEXNODE_SPLIT);
        ENSURE(ctx->nodes[nodeIndex].nodeKind != REGEXNODE_EMPTY);

        ENSURE(ctx->matchState[nodeIndex].isActive);
        FILEPOS earliestMatchPos = ctx->matchState[nodeIndex].earliestMatchPos;

        int nextNodeIndex = ctx->nodes[nodeIndex].nextIndex;
        set_active(ctx, nextNodeIndex, earliestMatchPos);
}

int extract_current_match(struct MatchCtx *matchCtx, FILEPOS *matchStartPos, FILEPOS *matchEndPos)
{
        if (!matchCtx->haveMatch)
                return 0;
        else {
                *matchStartPos = matchCtx->matchStartPos;
                *matchEndPos = matchCtx->matchEndPos;
                return 1;
        }
}

static void clear_next_matchstates(struct MatchCtx *matchCtx)
{
        for (int i = 0; i < matchCtx->numNodes; i++) {
                matchCtx->nextMatchState[i].isActive = 0;
                matchCtx->nextMatchState[i].earliestMatchPos = 666666666; // undefined
        }
}

void setup_matchctx(struct MatchCtx *matchCtx, struct Regex *regex)
{
        matchCtx->nodes = regex->nodes;
        matchCtx->numNodes = regex->numNodes;
        matchCtx->initialNodeIndex = regex->initialNodeIndex;
        matchCtx->haveMatch = 0;
        ALLOC_MEMORY(&matchCtx->matchState, matchCtx->numNodes);
        ALLOC_MEMORY(&matchCtx->nextMatchState, matchCtx->numNodes);
        clear_next_matchstates(matchCtx);
        ENSURE(matchCtx->nodes[matchCtx->initialNodeIndex].nodeKind == REGEXNODE_EMPTY);

        // some regexes match the empty string. With the current interface,
        // which consists basically of feed_character_into_regex_search(),
        // we need to start the search right now.
        FILEPOS currentFilepos = 0; // XXX the current interface is wrong since we can't specify where we start without feeding an actual character.
        matchCtx->matchStartPos = currentFilepos;
        matchCtx->matchEndPos = currentFilepos;
        set_active(matchCtx, matchCtx->initialNodeIndex, currentFilepos);
}

void teardown_matchctx(struct MatchCtx *matchCtx)
{
        FREE_MEMORY(&matchCtx->matchState);
        FREE_MEMORY(&matchCtx->nextMatchState);
}

void feed_character_into_regex_search(struct MatchCtx *ctx, int c, FILEPOS currentFilepos)
{
        ctx->matchEndPos = currentFilepos;  //XXX

        // is this the right place?
        ctx->haveMatch = 0;
        set_active(ctx, ctx->initialNodeIndex, currentFilepos);

        /* switch to next char */
        struct NodeMatchState *tmp = ctx->matchState;
        ctx->matchState = ctx->nextMatchState;
        ctx->nextMatchState = tmp;
        clear_next_matchstates(ctx);
        /**/

        //log_postf("feed character '%c'", c);

        for (int nodeIndex = 0; nodeIndex < ctx->numNodes; nodeIndex++) {
                if (!ctx->matchState[nodeIndex].isActive)
                        continue;
                struct RegexNode *node = &ctx->nodes[nodeIndex];
                switch (node->nodeKind) {
                case REGEXNODE_SPECIFIC_CHARACTER:
                        if (c == node->data.tSpecificCharacter.character) {
                                //log_postf("match specific character.");
                                set_successors_active(ctx, nodeIndex);
                        }
                        break;
                case REGEXNODE_ANY_CHARACTER:
                        //log_postf("match any character.");
                        set_successors_active(ctx, nodeIndex);
                        break;
                }
        }
}
