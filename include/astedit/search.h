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
        int *activeNodes;
        int *nextActiveNodes;
        /* For each index into matchNodes, the earliest position of all the
         * positions in the matched text that lead us to that MatchNode.
         * If the MatchNode is currently not active, the value will be -1. */
        FILEPOS *earliestStartPosition;
        FILEPOS *nextEarliestStartPosition;
        struct MatchNode *matchNodes;
        int numberOfNodes;
        int numActiveNodes;
        int numNextActiveNodes;
        /* the earliest-start-position of the last recorded match (or -1 if
         * none) */
        FILEPOS earliestMatch;
        /* curent byte position in text file to be matched */
        FILEPOS bytePosition;
};

void compile_pattern_from_fixed_string(struct CompiledPattern *pattern, const char *fixed, int length);
void free_pattern(struct CompiledPattern *pattern);

void feed_character_into_search(struct MatchState *state, int character);
void init_pattern_match(const struct CompiledPattern *pattern, struct MatchState *state, FILEPOS startpos);
void cleanup_pattern_match(struct MatchState *state);
//XXX should this be here?
void match_pattern(const struct CompiledPattern *pattern, const char *text, int length);

#endif
