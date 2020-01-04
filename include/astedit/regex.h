#ifndef ASTEDIT_REGEX_H_INCLUDED
#define ASTEDIT_REGEX_H_INCLUDED

#include <astedit/filepositions.h>

enum {  /* Nodes that are used for matching */
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

enum {
        REGEX_GROUP_SINGLECHARACTER,  // TODO: utf-8
        REGEX_GROUP_PARENTHESES,
};

struct Regex {
        struct RegexNode *nodes;
        int numNodes;
        int initialNodeIndex;
};

/* For each node in a compiled pattern, a corresponding state that is needed in
 * a match. */
struct NodeMatchState {
        int isActive;
        /* The earliest fileposition (since starting the search) that lead to
         * this node being active. */
        FILEPOS earliestMatchPos;
};

struct MatchCtx {
        struct RegexNode *nodes;
        int numNodes;
        int initialNodeIndex;

        int haveMatch;

        FILEPOS matchStartPos;
        FILEPOS matchEndPos;

        struct NodeMatchState *matchState;
        struct NodeMatchState *nextMatchState;
};

void setup_regex(struct Regex *regex);  // remove this? it's only zero-allocation
void teardown_regex(struct Regex *regex);
int compile_regex_from_pattern(struct Regex *regex, const char *pattern, int length);

void setup_matchctx(struct MatchCtx *matchCtx, struct Regex *regex);
void teardown_matchctx(struct MatchCtx *matchCtx);
void feed_character_into_regex_search(struct MatchCtx *ctx, int c, FILEPOS currentFilepos);

/* reports whether there is a match */
int extract_current_match(struct MatchCtx *matchCtx, FILEPOS *matchStartPos, FILEPOS *matchEndPos);

int match_regex(struct MatchCtx *matchCtx, const char *string, int length);

#endif
