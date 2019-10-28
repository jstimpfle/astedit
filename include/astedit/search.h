#ifndef ASTEDIT_SEARCH_H_INCLUDED
#define ASTEDIT_SEARCH_H_INCLUDED

#include <astedit/filepositions.h>

enum {
        MATCH_BEGINOFSTRING,
        MATCH_ENDOFSTRING,
        MATCH_SUCCESS,
        MATCH_CHARACTER,
};

enum {
        INDEX_LASTINCHAIN = -1,
};

struct MatchNode {
        char matchKind;
        char characterToMatch;
        int firstSuccessorIndex;
        int alternativeNodeIndex;
};

struct CompiledPattern {
        /* matchNodes[0] is the starting node */
        struct MatchNode *matchNodes;
        int numberOfNodes;
};

struct MatchState {
        int *activeNodes;  // restrict?
        int *nextActiveNodes;  // restrict?
        int *isInNextActiveNodes;  // restrict?
        struct MatchNode *matchNodes;
        int numberOfNodes;
        int numActiveNodes;
        int numNextActiveNodes;
        /**/
        int haveMatch;
        FILEPOS lastMatchPosition;
};

int feed_character_into_search(struct MatchState *state, int character);
void init_pattern_match(const struct CompiledPattern *pattern, struct MatchState *state);
void cleanup_pattern_match(struct MatchState *state);
//XXX should this be here?
int match_pattern(const struct CompiledPattern *pattern, const char *text, int length);

#endif
