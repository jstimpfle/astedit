#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/logging.h>
#include <astedit/search.h>

static void add_all_alternatives(struct MatchState *state, int firstIndex, FILEPOS startPosition)
{
        for (int index = firstIndex;
             index != INDEX_LASTINCHAIN;
             index = state->matchNodes[index].alternativeNodeIndex)
        {
                if (state->nextEarliestStartPosition[index] == -1) {
                        state->nextEarliestStartPosition[index] = startPosition;
                        state->nextActiveNodes[state->numNextActiveNodes++] = index;
                }
                else if (state->nextEarliestStartPosition[index] > startPosition) {
                        state->nextEarliestStartPosition[index] = startPosition;
                }
                int isSuccessNode = state->matchNodes[index].matchKind == MATCH_SUCCESS;
                if (isSuccessNode) {
                        FILEPOS earliest = state->nextEarliestStartPosition[index];
                        if (state->earliestMatch == -1 || state->earliestMatch > earliest)
                                state->earliestMatch = earliest;
                }
        }
}

static void add_all_successors(struct MatchState *state, int nodeIndex)
{
        FILEPOS startPosition = state->earliestStartPosition [nodeIndex];
        int firstIndex = state->matchNodes[nodeIndex].firstSuccessorIndex;
        add_all_alternatives(state, firstIndex, startPosition);
}

void feed_character_into_search(struct MatchState *state, int character)
{
        add_all_alternatives(state, 0, state->bytePosition);  /* XXX: start with chain that starts with index 0. That currently means start-of-regex */
        state->bytePosition ++;
        for (int i = 0; i < state->numNextActiveNodes; i++)
                ENSURE(state->nextEarliestStartPosition[state->nextActiveNodes[i]] != -1);
        /* swap the two arrays */
#define SWAP(type, a, b) { type tmp = (a); (a) = (b); (b) = tmp; }
        SWAP(int*, state->activeNodes, state->nextActiveNodes);
        SWAP(int, state->numActiveNodes, state->numNextActiveNodes);
        SWAP(FILEPOS*, state->earliestStartPosition, state->nextEarliestStartPosition);
#undef SWAP
        /* Reset next-active-nodes array and start position info */
        for (int i = 0; i < state->numNextActiveNodes; i++) {
                int nodeIndex = state->nextActiveNodes[i];
                ENSURE(state->nextEarliestStartPosition[nodeIndex] != -1);
                state->nextEarliestStartPosition[nodeIndex] = -1;
        }
        state->numNextActiveNodes = 0;
        /* Compute next-active-nodes array based on current (i.e. old-next)
         * active-nodes array. */
        for (int i = 0; i < state->numActiveNodes; i++) {
                int nodeIndex = state->activeNodes[i];
                ENSURE(state->earliestStartPosition[nodeIndex] != -1);
                if (state->matchNodes[nodeIndex].matchKind == MATCH_CHARACTER) {
                        if (state->matchNodes[nodeIndex].characterToMatch == character) {
                                add_all_successors(state, nodeIndex);
                        }
                }
        }
}

void init_pattern_match(const struct CompiledPattern *pattern, struct MatchState *state)
{
        state->matchNodes = pattern->matchNodes;
        state->numberOfNodes = pattern->numberOfNodes;
        ALLOC_MEMORY(&state->activeNodes, state->numberOfNodes);
        ALLOC_MEMORY(&state->nextActiveNodes, state->numberOfNodes);
        ALLOC_MEMORY(&state->earliestStartPosition, state->numberOfNodes);
        ALLOC_MEMORY(&state->nextEarliestStartPosition, state->numberOfNodes);
        state->numActiveNodes = 0;
        state->numNextActiveNodes = 0;
        for (int i = 0; i < state->numberOfNodes; i++) {
                state->earliestStartPosition[i] = -1;
                state->nextEarliestStartPosition[i] = -1;
        }
        state->earliestMatch = -1;
        state->bytePosition = 0;
}

void cleanup_pattern_match(struct MatchState *state)
{
        FREE_MEMORY(&state->activeNodes);
        FREE_MEMORY(&state->nextActiveNodes);
        FREE_MEMORY(&state->earliestStartPosition);
        FREE_MEMORY(&state->nextEarliestStartPosition);
}

void match_pattern(const struct CompiledPattern *pattern, const char *text, int length)
{
        struct MatchState matchState;
        struct MatchState *state = &matchState;
        init_pattern_match(pattern, state);
        for (int i = 0; i < length; i++)
                feed_character_into_search(state, text[i]);
        cleanup_pattern_match(state);
}
