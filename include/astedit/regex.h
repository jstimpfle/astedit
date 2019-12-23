#ifndef ASTEDIT_REGEX_H_INCLUDED
#define ASTEDIT_REGEX_H_INCLUDED

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


struct MatchCtx {
        struct RegexNode *nodes;
        int numNodes;

        int initialNodeIndex;
        int haveMatch;
        int *isActive;
        int *nextIsActive;
};

void setup_readctx(struct RegexReadCtx *ctx, const char *pattern);
void teardown_readctx(struct RegexReadCtx *ctx);
void read_pattern(struct RegexReadCtx *ctx);

void setup_matchctx_from_readctx(struct MatchCtx *matchCtx, struct RegexReadCtx *readCtx);
void setup_matchctx_from_pattern(struct MatchCtx *matchCtx, const char *pattern);
void teardown_matchctx(struct MatchCtx *matchCtx);
void feed_character_into_regex_search(struct MatchCtx *ctx, int c);
int match_regex(struct MatchCtx *matchCtx, const char *string, int length);

#endif
