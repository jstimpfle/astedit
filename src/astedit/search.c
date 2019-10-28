#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/logging.h>
#include <astedit/search.h>
#include <stdio.h>
#include <string.h>

static int add_all_alternatives(struct MatchState *state, int firstNode)
{
        for (int nextIndex = firstNode;
             nextIndex != INDEX_LASTINCHAIN;
             nextIndex = state->matchNodes[nextIndex].alternativeNodeIndex)
        {
                if (!state->isInNextActiveNodes[nextIndex]) {
                        if (state->matchNodes[nextIndex].matchKind == MATCH_SUCCESS)
                                return 1;
                        state->isInNextActiveNodes[nextIndex] = 1;
                        state->nextActiveNodes[state->numNextActiveNodes++] = nextIndex;
                }
        }
        return 0;
}

int feed_character_into_search(struct MatchState *state, int character)
{
        add_all_alternatives(state, 0);  /* start with chain that starts with 0 */
        /* swap the two arrays */
        {
                int *tmp = state->activeNodes;
                state->activeNodes = state->nextActiveNodes;
                state->nextActiveNodes = tmp;
        }
        {
                int tmp = state->numActiveNodes;
                state->numActiveNodes = state->numNextActiveNodes;
                state->numNextActiveNodes = tmp;
        }
        /**/
        state->numNextActiveNodes = 0;
        for (int j = 0; j < state->numberOfNodes; j++) {
                // we could optimize that by only visiting the next-active nodes through nextActiveNodes
                state->isInNextActiveNodes[j] = 0;
        }
        for (int j = 0; j < state->numActiveNodes; j++) {
                int nodeIndex = state->activeNodes[j];
                struct MatchNode *node = &state->matchNodes[nodeIndex];
                if (node->matchKind == MATCH_CHARACTER) {
                        if (node->characterToMatch == character) {
                                int haveMatch = add_all_alternatives(state, node->firstSuccessorIndex);
                                if (haveMatch)
                                        return 1;
                        }
                }
        }
        return 0;
}

void init_pattern_match(const struct CompiledPattern *pattern, struct MatchState *state)
{
        state->matchNodes = pattern->matchNodes;
        state->numberOfNodes = pattern->numberOfNodes;
        state->numActiveNodes = 0;
        state->numNextActiveNodes = 0;
        ALLOC_MEMORY(&state->activeNodes, state->numberOfNodes);
        ALLOC_MEMORY(&state->nextActiveNodes, state->numberOfNodes);
        ALLOC_MEMORY(&state->isInNextActiveNodes, state->numberOfNodes);
        ZERO_ARRAY(state->activeNodes, state->numberOfNodes);
        ZERO_ARRAY(state->nextActiveNodes, state->numberOfNodes);
        ZERO_ARRAY(state->isInNextActiveNodes, state->numberOfNodes);
}

void cleanup_pattern_match(struct MatchState *state)
{
        FREE_MEMORY(&state->activeNodes);
        FREE_MEMORY(&state->nextActiveNodes);
        FREE_MEMORY(&state->isInNextActiveNodes);
}

int match_pattern(const struct CompiledPattern *pattern, const char *text, int length)
{
        struct MatchState matchState;
        struct MatchState *state = &matchState;
        init_pattern_match(pattern, state);

        int haveMatch = 0;
        for (int i = 0; i < length; i++) {
                if (feed_character_into_search(state, text[i])) {
                        haveMatch = 1;
                        goto out;
                }
        }
        for (int i = 0; i < state->numNextActiveNodes; i++) {
                int nodeIndex = state->nextActiveNodes[i];
                if (state->matchNodes[nodeIndex].matchKind == MATCH_SUCCESS) {
                        haveMatch = 1;
                        goto out;
                }
        }
out:
        cleanup_pattern_match(state);
        return haveMatch;
}
