#include <rb3ptr.h>
#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memory.h>
#include <astedit/bytes.h>
#include <astedit/logging.h>
#include <astedit/textrope.h>
#include <stddef.h>  // offsetof

static inline int is_utf8_leader_byte(int c) { return (c & 0xc0) != 0x80; }



/* We try to make nodes as big as possible but no bigger than this.
We do this by combining adjacent nodes whose combined size does not exceed
this. This means that each node will always be at least half that size. */
enum {
        TARGET_LENGTH = 8192,
};

struct Textnode {
        struct rb3_head head;
        char *text;
        int ownLength;  /* length in bytes */
        int ownLines;  /* 0x0a count */
        int ownCodepoints;  /* UTF-8 leader-byte count */
        FILEPOS totalLength;
        FILEPOS totalLines;
        FILEPOS totalCodepoints;
};

struct Textrope {
        /* XXX: this is some trickery to avoid UB. TODO Think about making this
         * nicer.  The wasted memory is neglibible. */
        union {
                struct rb3_tree tree;
                struct Textnode baseNode;
        } tree;
};



static inline struct Textnode *textnode_from_head(struct rb3_head *head)
{
        return (struct Textnode *) (((char *)head) - offsetof(struct Textnode, head));
}

static struct Textnode *get_root_node(struct Textrope *rope)
{
        struct rb3_head *root = rb3_get_root(&rope->tree.tree);
        return root ? textnode_from_head(root) : NULL;
}

static struct Textnode *get_leftmost_node(struct Textrope *rope)
{
        struct rb3_head *head = rb3_get_min(&rope->tree.tree);
        return head ? textnode_from_head(head) : NULL;
}

static struct Textnode *get_child(struct Textnode *node, int dir)
{
        struct rb3_head *childHead = rb3_get_child(&node->head, dir);
        return childHead ? textnode_from_head(childHead) : NULL;
}

static struct Textnode *get_prev(struct Textnode *node)
{
        struct rb3_head *prevHead = rb3_get_prev(&node->head);
        return prevHead ? textnode_from_head(prevHead) : NULL;
}

static struct Textnode *get_next(struct Textnode *node)
{
        struct rb3_head *nextHead = rb3_get_next(&node->head);
        return nextHead ? textnode_from_head(nextHead) : NULL;
}

static inline FILEPOS node_length(struct Textnode *node)
{
        return node ? node->totalLength : 0;
}

static inline FILEPOS node_lines(struct Textnode *node)
{
        return node ? node->totalLines : 0;
}

static inline FILEPOS node_codepoints(struct Textnode *node)
{
        return node ? node->totalCodepoints : 0;
}


static struct Textnode *create_textnode(void)
{
        struct Textnode *node;
        ALLOC_MEMORY(&node, 1);
        ZERO_MEMORY(&node->head);
        ALLOC_MEMORY(&node->text, TARGET_LENGTH);
        node->text[0] = 0;
        node->ownLength = 0;
        node->totalLength = 0;
        node->ownLines = 0;
        node->totalLines = 0;
        node->ownCodepoints = 0;
        node->totalCodepoints = 0;
        return node;
}

static void update_textnode_with_child(struct Textnode *node, int linkDir)
{
        struct Textnode *child = get_child(node, linkDir);
        if (child != NULL) {
                node->totalLength += child->totalLength;
                node->totalLines += child->totalLines;
                node->totalCodepoints += child->totalCodepoints;
        }
}

static void augment_helper(struct rb3_head *head)
{
        struct Textnode *node = textnode_from_head(head);
        node->totalLength = node->ownLength;
        node->totalLines = node->ownLines;
        node->totalCodepoints = node->ownCodepoints;
        update_textnode_with_child(node, RB3_LEFT);
        update_textnode_with_child(node, RB3_RIGHT);
}

static void update_textnode_after_change(struct Textnode *node)
{
        rb3_update_augment(&node->head, &augment_helper);
}

static void link_textnode(struct Textnode *child, struct Textnode *parent, int linkDir)
{
        rb3_link_and_rebalance_and_augment(&child->head, &parent->head, linkDir, &augment_helper);
}

static void unlink_textnode(struct Textnode *node)
{
        rb3_unlink_and_rebalance_and_augment(&node->head, &augment_helper);
}

static void link_textnode_next_to(struct Textnode *newNode, struct Textnode *node, int dir)
{
        struct rb3_head *head = &node->head;
        if (rb3_has_child(head, dir)) {
                head = rb3_get_prevnext_descendant(head, dir);
                dir = !dir;
        }
        link_textnode(newNode, textnode_from_head(head), dir);
}

FILEPOS textrope_length(struct Textrope *rope)
{
        return node_length(get_root_node(rope));
}

FILEPOS textrope_number_of_lines(struct Textrope *rope)
{
        return node_lines(get_root_node(rope));
}

FILEPOS textrope_number_of_lines_quirky(struct Textrope *rope)
{
        FILEPOS numLines = textrope_number_of_lines(rope);
        FILEPOS textLength = textrope_length(rope);
        if (textLength > 0) {
                int c = textrope_read_char_at(rope, textLength - 1);
                if (c != '\n')
                        return numLines + 1;
        }
        return numLines;
}

FILEPOS textrope_number_of_codepoints(struct Textrope *rope)
{
        return node_codepoints(get_root_node(rope));
}


static void destroy_textnode(struct Textnode *textnode)
{
        FREE_MEMORY(&textnode->text);
        FREE_MEMORY(&textnode);
}

static void destroy_subtree(struct Textnode *node)
{
        if (node != NULL) {
                destroy_subtree(get_child(node, RB3_LEFT));
                destroy_subtree(get_child(node, RB3_RIGHT));
                destroy_textnode(node);
        }
}

struct Textrope *create_textrope(void)
{
        struct Textrope *textrope;
        ALLOC_MEMORY(&textrope, 1);
        rb3_reset_tree(&textrope->tree.tree);
        return textrope;
}

void destroy_textrope(struct Textrope *textrope)
{
        destroy_subtree(get_root_node(textrope));
        FREE_MEMORY(&textrope);
}




struct Textiter {
        /* reference to node */
        struct Textnode *current;
        /* parent and childDir together define a place where a reference to a node is stored. */
        struct Textnode *parent;
        int linkDir;
        /* holds the text position and line number of the first byte of the referenced node's text. */
        FILEPOS pos;
        FILEPOS line;
        FILEPOS codepointPosition;
};

static void init_textiter(struct Textiter *iter, struct Textrope *rope)
{
        iter->current = get_root_node(rope);
        iter->parent = (struct Textnode *) &rope->tree.baseNode;
        iter->linkDir = RB3_LEFT;
        if (iter->current == NULL) {
                iter->pos = 0;
                iter->line = 0;
                iter->codepointPosition = 0;
        }
        else {
                struct Textnode *left = get_child(iter->current, RB3_LEFT);
                if (left == NULL) {
                        iter->pos = 0;
                        iter->line = 0;
                        iter->codepointPosition = 0;
                }
                else {
                        iter->pos = left->totalLength;
                        iter->line = left->totalLines;
                        iter->codepointPosition = left->totalCodepoints;
                }
        }
}

static void go_to_left_child(struct Textiter *iter)
{
        ENSURE(iter->current != NULL);
        struct Textnode *child = get_child(iter->current, RB3_LEFT);
        iter->parent = iter->current;
        iter->current = child;
        iter->linkDir = RB3_LEFT;
        if (child == NULL)
                return;
        iter->pos -= child->ownLength;
        iter->line -= child->ownLines;
        iter->codepointPosition -= child->ownCodepoints;
        struct Textnode *grandchild = get_child(child, RB3_RIGHT);
        if (grandchild == NULL)
                return;
        iter->pos -= grandchild->totalLength;
        iter->line -= grandchild->totalLines;
        iter->codepointPosition -= grandchild->totalCodepoints;
}

static void go_to_right_child(struct Textiter *iter)
{
        ENSURE(iter->current != NULL);
        iter->pos += iter->current->ownLength;
        iter->line += iter->current->ownLines;
        iter->codepointPosition += iter->current->ownCodepoints;
        struct Textnode *child = get_child(iter->current, RB3_RIGHT);
        iter->parent = iter->current;
        iter->current = child;
        iter->linkDir = RB3_RIGHT;
        if (child == NULL)
                return;
        struct Textnode *grandchild = get_child(child, RB3_LEFT);
        if (grandchild == NULL)
                return;
        iter->pos += grandchild->totalLength;
        iter->line += grandchild->totalLines;
        iter->codepointPosition += grandchild->totalCodepoints;
}



static int is_last_byte_end_of_line(struct Textnode *node)
{
        ENSURE(node->ownLength > 0);
        return node->text[node->ownLength - 1] == '\n';
}

static int is_first_byte_start_of_line(struct Textnode *node)
{
        struct Textnode *prev = get_prev(node);
        return prev == NULL || is_last_byte_end_of_line(prev);
}


static struct Textiter find_first_node_that_contains_the_character_at_pos(struct Textrope *rope, FILEPOS pos)
{
        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);
        while (iter->current != NULL) {
                if (iter->pos > pos)
                        go_to_left_child(iter);
                else if (iter->pos + iter->current->ownLength <= pos)
                        go_to_right_child(iter);
                else
                        break;
        }
        return textiter;
}

static struct Textiter find_first_node_that_contains_the_given_line(struct Textrope *rope, FILEPOS line)
{
        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);
        while (iter->current != NULL) {
                if (iter->line > line)
                        go_to_left_child(iter);
                else if (iter->line == line && !is_first_byte_start_of_line(iter->current))
                        go_to_left_child(iter);
                else if (iter->line + iter->current->ownLines < line)
                        go_to_right_child(iter);
                else if (iter->line + iter->current->ownLines == line && is_last_byte_end_of_line(iter->current))
                        go_to_right_child(iter);
                else
                        break;
        }
        return textiter;
}

static struct Textiter find_first_node_that_contains_the_given_codepointPos(struct Textrope *rope, FILEPOS codepointPos)
{
        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);
        while (iter->current != NULL) {
                if (iter->codepointPosition > codepointPos)
                        go_to_left_child(iter);
                else if (iter->codepointPosition + iter->current->ownCodepoints <= codepointPos)
                        go_to_right_child(iter);
                else
                        break;
        }
        return textiter;
}


static int compute_internal_distance(struct Textiter *iter, FILEPOS pos)
{
        ENSURE(iter->pos <= pos);
        ENSURE(iter->pos + TARGET_LENGTH >= pos);

        // When I checked, this situation really could happen and that's ok.
        /*if (iter->pos + TARGET_LENGTH == pos)
                log_postf("Can that really happen?");
                */

        return cast_filepos_to_int(pos - iter->pos);
}




void compute_line_number_and_codepoint_position(
        struct Textrope *rope, FILEPOS pos, FILEPOS *outLinenumber, FILEPOS *outCodepointPosition)
{
        ENSURE(0 <= pos && pos <= textrope_length(rope));
        if (pos == textrope_length(rope)) {
                *outLinenumber = textrope_number_of_lines(rope);
                *outCodepointPosition = textrope_number_of_codepoints(rope);
                return;
        }
        struct Textiter iter = find_first_node_that_contains_the_character_at_pos(rope, pos);
        ENSURE(iter.current != NULL);
        FILEPOS currentPos = iter.pos;
        FILEPOS currentLine = iter.line;
        FILEPOS currentCodepoint = iter.codepointPosition;
        for (int i = 0; ; i++) {
                if (currentPos == pos)
                        break;
                ENSURE(i < iter.current->ownLength);
                int c = iter.current->text[i];
                if (is_utf8_leader_byte(c))
                        currentCodepoint++;
                currentPos++;
                if (c == '\n')
                        currentLine++;
        }
        *outLinenumber = currentLine;
        *outCodepointPosition = currentCodepoint;
}

void compute_pos_and_line_number_from_codepoint(
        struct Textrope *rope, FILEPOS codepointPos, FILEPOS *outPos, FILEPOS *outLineNumber)
{
        ENSURE(0 <= codepointPos && codepointPos <= textrope_number_of_codepoints(rope));
        if (codepointPos == textrope_number_of_codepoints(rope)) {
                *outPos = textrope_length(rope);
                *outLineNumber = textrope_number_of_codepoints(rope);
                return;
        }
        struct Textiter iter = find_first_node_that_contains_the_given_codepointPos(rope, codepointPos);
        ENSURE(iter.current != NULL);  // caller should check range first
        FILEPOS currentPos = iter.pos;
        FILEPOS currentCodepoint = iter.codepointPosition;
        FILEPOS currentLine = iter.line;
        /* Codepoints are a little tricky / different. The codepoint with index
         * N is not located at the byte that immediately follows the N'th leader
         * byte, but at the byte that has the (N+1)'s leaderbyte. */
        for (int i = 0; ; i++) {
                ENSURE(i < iter.current->ownLength);
                int c = iter.current->text[i];
                if (is_utf8_leader_byte(c)) {
                        if (currentCodepoint == codepointPos)
                                break;
                        currentCodepoint++;
                }
                currentPos++;
                if (c == '\n')
                        currentLine++;
        }
        *outPos = currentPos;
        *outLineNumber = currentLine;
}

void compute_pos_and_codepoint_of_line(struct Textrope *rope, FILEPOS lineNumber, FILEPOS *outPos, FILEPOS *outCodepointPos)
{
        ENSURE(lineNumber <= textrope_number_of_lines_quirky(rope));
        if (lineNumber == textrope_number_of_lines_quirky(rope)) {
                *outPos = textrope_length(rope);
                *outCodepointPos = textrope_number_of_codepoints(rope);
                return;
        }
        struct Textiter iter = find_first_node_that_contains_the_given_line(rope, lineNumber);
        FILEPOS currentPos = iter.pos;
        FILEPOS codepointPosition = iter.codepointPosition;
        FILEPOS currentLine = iter.line;
        ENSURE(iter.current != 0);
        for (int i = 0; ; i++) {
                ENSURE(i < iter.current->ownLength); // I believe this can break with "quirky" lines
                if (currentLine == lineNumber)
                        break;
                int c = iter.current->text[i];
                if (is_utf8_leader_byte(c))
                        codepointPosition++;
                currentPos++;
                if (c == '\n')
                        currentLine++;
        }
        *outPos = currentPos;
        *outCodepointPos = codepointPosition;
}

FILEPOS compute_codepoint_position(struct Textrope *rope, FILEPOS pos)
{
        FILEPOS lineNumber;
        FILEPOS codepointPosition;
        compute_line_number_and_codepoint_position(rope, pos, &lineNumber, &codepointPosition);
        return codepointPosition;
}

FILEPOS compute_line_number(struct Textrope *rope, FILEPOS pos)
{
        FILEPOS lineNumber;
        FILEPOS codepointPosition;
        compute_line_number_and_codepoint_position(rope, pos, &lineNumber, &codepointPosition);
        return lineNumber;
}

FILEPOS compute_pos_of_codepoint(struct Textrope *rope, FILEPOS codepointPos)
{
        FILEPOS pos;
        FILEPOS lineNumber;
        compute_pos_and_line_number_from_codepoint(rope, codepointPos, &pos, &lineNumber);
        return pos;
}

FILEPOS compute_pos_of_line(struct Textrope *rope, FILEPOS lineNumber)
{
        FILEPOS position;
        FILEPOS codepointPosition;
        compute_pos_and_codepoint_of_line(rope, lineNumber, &position, &codepointPosition);
        return position;
}

FILEPOS compute_codepoint_of_line(struct Textrope *rope, FILEPOS lineNumber)
{
        FILEPOS position;
        FILEPOS codepointPosition;
        compute_pos_and_codepoint_of_line(rope, lineNumber, &position, &codepointPosition);
        return codepointPosition;
}

FILEPOS compute_pos_of_line_end(struct Textrope *rope, FILEPOS lineNumber)
{
        ENSURE(lineNumber >= 0);
        ENSURE(lineNumber < textrope_number_of_lines_quirky(rope));
        FILEPOS numberOfLines = textrope_number_of_lines(rope);
        if (lineNumber == numberOfLines) {
                // due to the assertion above we know that the last character is
                // not a newline character.
                FILEPOS length = textrope_length(rope);
                if (length == 0)
                        return 0;
                else
                        return length - 1;
        }
        else {
                FILEPOS linePos = compute_pos_of_line(rope, lineNumber);
                // we can expect a newline at the end of the line.
                FILEPOS pos = compute_pos_of_line(rope, lineNumber + 1) - 1;
                ENSURE(textrope_read_char_at(rope, pos) == '\n');
                if (pos > linePos)
                        return pos - 1;
                return pos;
        }
}



static void count_lines_and_codepoints(const char *text, int length,
        int *outNumlines, int *outNumCodepoints)
{
        int numLines = 0;
        int numCodepoints = 0;
        for (int i = 0; i < length; i++) {
                if (text[i] == '\n')
                        numLines++;
                if (is_utf8_leader_byte(text[i]))
                        numCodepoints++;
        }
        *outNumlines = numLines;
        *outNumCodepoints = numCodepoints;
}

static void added_range(struct Textnode *node, const char *text, int length)
{
        int numLines;
        int numCodepoints;
        count_lines_and_codepoints(text, length, &numLines, &numCodepoints);
        node->ownLines += numLines;
        node->ownCodepoints += numCodepoints;
}

static void erased_range(struct Textnode *node, const char *text, int length)
{
        int numLines;
        int numCodepoints;
        count_lines_and_codepoints(text, length, &numLines, &numCodepoints);
        node->ownLines -= numLines;
        node->ownCodepoints -= numCodepoints;
}






struct ChainStream {
        const char *text;
        FILEPOS readpos;
        FILEPOS endpos;
        struct ChainStream *next;
};

struct StreamsChain {
        struct ChainStream *streams;
        FILEPOS remainingBytes;
        FILEPOS totalLength;
};

static void init_StreamsChain(struct StreamsChain *sc)
{
        sc->streams = NULL;
        sc->remainingBytes = 0;
        sc->totalLength = 0;
}

static void append_to_StreamsChain(struct StreamsChain *sc, struct ChainStream *stream)
{
        ENSURE(stream->next == NULL);
        struct ChainStream **ptr = &sc->streams;
        while (*ptr)
                ptr = &(*ptr)->next;
        *ptr = stream;
        sc->totalLength += stream->endpos - stream->readpos;
        sc->remainingBytes += stream->endpos - stream->readpos;
}

static void copy_from_StreamsChain(struct StreamsChain *sc, char *dst, int nbytes)
{
        ENSURE(sc->remainingBytes >= nbytes);
        int todoBytes = nbytes;
        while (todoBytes > 0) {
                struct ChainStream *stream = sc->streams;
                ENSURE(stream != NULL);
                FILEPOS n = stream->endpos - stream->readpos;
                if (n > todoBytes)
                        n = todoBytes;
                int toCopyNow = cast_filepos_to_int(n);
                copy_memory(dst, stream->text + stream->readpos, toCopyNow);
                dst += toCopyNow;
                todoBytes -= toCopyNow;
                stream->readpos += toCopyNow;
                if (stream->readpos == stream->endpos)
                        sc->streams = stream->next;
        }
        sc->remainingBytes -= nbytes;
}


static void copy_from_StreamsChain_to_Textnode(struct StreamsChain *sc, struct Textnode *node, int offset, int length)
{
        //XXX
        if (length > sc->remainingBytes)
                length = cast_filepos_to_int(sc->remainingBytes);
        copy_from_StreamsChain(sc, node->text + offset, length);
        added_range(node, node->text + offset, length);
        node->ownLength += length;
}







void insert_text_into_textrope(struct Textrope *rope, FILEPOS pos, const char *text, FILEPOS length)
{
        ENSURE(pos <= textrope_length(rope));

        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);

        /* if the tree is currently empty, make a node with content length 0. */
        if (iter->current == NULL) {
                ENSURE(pos == 0);
                struct Textnode *node = create_textnode();
                link_textnode(node, iter->parent, iter->linkDir);
                init_textiter(iter, rope);
        }

        /* find the node that either properly contains the offset pos or whose
        right border is at pos. */
        for (;;) {
                if (iter->pos > pos)
                        go_to_left_child(iter);
                else if (iter->pos + iter->current->ownLength < pos)
                        go_to_right_child(iter);
                else
                        break;
        }

        ENSURE(iter->current != NULL);

        struct Textnode *node = iter->current;
        int internalPos = compute_internal_distance(iter, pos);
        ENSURE(0 <= internalPos && internalPos <= node->ownLength);

        /* move the data after the insert position to temporary storage. */
        char tuck[TARGET_LENGTH];
        int tuckLength = node->ownLength - internalPos;
        ENSURE(0 <= tuckLength && tuckLength <= TARGET_LENGTH);
        copy_memory(tuck, node->text + internalPos, tuckLength);
        erased_range(node, node->text + internalPos, tuckLength);
        node->ownLength = internalPos;

        struct ChainStream stream1;
        stream1.text = text;
        stream1.readpos = 0;
        stream1.endpos = length;
        stream1.next = NULL;

        struct ChainStream stream2;
        stream2.text = tuck;
        stream2.readpos = 0;
        stream2.endpos = tuckLength;
        stream2.next = NULL;

        struct StreamsChain chain;
        init_StreamsChain(&chain);
        append_to_StreamsChain(&chain, &stream1);
        append_to_StreamsChain(&chain, &stream2);

        int finalBytesAvailable;
        {
                struct Textnode *succ = get_next(node);
                finalBytesAvailable = succ == NULL ? 0 : TARGET_LENGTH - succ->ownLength;
        }

        copy_from_StreamsChain_to_Textnode(&chain, node, node->ownLength, TARGET_LENGTH - node->ownLength);
        update_textnode_after_change(node);

        while (chain.remainingBytes > finalBytesAvailable) {
                struct Textnode *lastNode = node;
                node = create_textnode();
                copy_from_StreamsChain_to_Textnode(&chain, node, node->ownLength, TARGET_LENGTH - node->ownLength);
                link_textnode_next_to(node, lastNode, RB3_RIGHT);
        }

        if (chain.remainingBytes > 0) {
                ENSURE(chain.remainingBytes <= finalBytesAvailable);
                int remainingBytes = cast_filepos_to_int(chain.remainingBytes);
                node = get_next(node);
                ENSURE(TARGET_LENGTH - node->ownLength == finalBytesAvailable);
                move_memory(node->text, remainingBytes, node->ownLength);
                copy_from_StreamsChain_to_Textnode(&chain, node, 0, remainingBytes);
                update_textnode_after_change(node);
        }
}

void erase_text_from_textrope(struct Textrope *rope, FILEPOS pos, FILEPOS length)
{
        ENSURE(0 <= pos);
        ENSURE(pos + length <= textrope_length(rope));

        struct Textiter textiter = find_first_node_that_contains_the_character_at_pos(rope, pos);
        struct Textiter *iter = &textiter;

        struct Textnode *node = iter->current;
        FILEPOS numDeleted = 0;

        /* partial delete from left node of range */
        if (pos != iter->pos) {
                int internalPos = compute_internal_distance(iter, pos);
                ENSURE(0 <= internalPos && internalPos < node->ownLength);
                int nd = node->ownLength - internalPos;
                if (nd > length - numDeleted)
                        nd = cast_filepos_to_int(length - numDeleted);

                erased_range(node, node->text + internalPos, nd);
                move_memory(node->text + internalPos + nd, -nd,
                        node->ownLength - internalPos - nd);
                numDeleted += nd;
                node->ownLength -= nd;
                /* We might optimize this augmentation away:
                Maybe we need to augment this node again later anyway. */
                update_textnode_after_change(node);
                node = get_next(node);
        }

        /* Unlink/delete all nodes whose contents are completely in the
        to-be-deleted range. */
        while (length != numDeleted) {
                ENSURE(node != NULL);

                if (node->ownLength > length - numDeleted)
                        break;

                numDeleted += node->ownLength;

                struct Textnode *nextNode = get_next(node);
                unlink_textnode(node);
                destroy_textnode(node);
                node = nextNode;
        }

        /* partial delete from last node */
        if (length > numDeleted) {
                ENSURE(node != NULL);
                int nd = cast_filepos_to_int(length - numDeleted);
                ENSURE(numDeleted + node->ownLength >= nd);
                /* Just erase the last chunk of text inside the last node */
                erased_range(node, node->text, nd);
                move_memory(node->text + nd, -nd, node->ownLength - nd);
                node->ownLength -= nd;
                update_textnode_after_change(node);
        }

        /* maybe merge last visited node with previous */
        if (node != NULL) {
                /* this may or may not be (depending on if we did a partial erase)
                the first node that we visited */
                struct Textnode *prevNode = get_prev(node);
                if (prevNode != NULL) {
                        if (prevNode->ownLength + node->ownLength <= TARGET_LENGTH) {
                                copy_memory(prevNode->text + prevNode->ownLength,
                                        node->text, node->ownLength);
                                added_range(prevNode, node->text, node->ownLength);
                                prevNode->ownLength += node->ownLength;
                                update_textnode_after_change(prevNode);
                                unlink_textnode(node);
                                destroy_textnode(node);
                        }
                }
        }
}

FILEPOS copy_text_from_textrope(struct Textrope *rope, FILEPOS offset, char *dstBuffer, FILEPOS length)
{
        ENSURE(0 <= offset);
        ENSURE(offset + length <= textrope_length(rope));

        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);

        while (iter->current) {
                if (iter->pos > offset)
                        go_to_left_child(iter);
                else if (iter->pos + iter->current->ownLength <= offset)
                        go_to_right_child(iter);
                else
                        break;
        }

        FILEPOS numRead = 0;

        struct Textnode *node = iter->current;
        if (node != NULL) {
                /* Read from first node - we might have to start somewhere in the middle */
                int internalPos = compute_internal_distance(iter, offset);
                ENSURE(0 <= internalPos && internalPos < node->ownLength);
                int numToRead = min_filepos_as_int(length, node->ownLength - internalPos);
                copy_memory(dstBuffer, node->text + internalPos, numToRead);
                numRead += numToRead;
                node = get_next(node);
        }

        /* Read subsequent nodes */
        while (node != NULL && numRead < length) {
                int numToRead = min_filepos_as_int(length - numRead, node->ownLength);
                copy_memory(dstBuffer + numRead, node->text, numToRead);
                numRead += numToRead;
                node = get_next(node);
        }

        return numRead;
}


int textrope_read_char_at(struct Textrope *rope, FILEPOS bytePosition)
{
        char c;
        copy_text_from_textrope(rope, bytePosition, &c, 1);
        return (int) (unsigned char) c;
}







static void check_node_values(struct Textnode *node)
{
        if (node == NULL)
                return;
        check_node_values(get_child(node, RB3_LEFT));
        check_node_values(get_child(node, RB3_RIGHT));

        if (node->ownLength == 0)
                fatal("Zero length node.\n");
        if (node->ownLength < 0 || node->ownLength > TARGET_LENGTH)
                fatal("Bad length.\n");
        if (node->ownLines < 0 || node->ownLines > node->ownLength)
                fatal("Bad lines.\n");
        if (node->ownCodepoints < 0 || node->ownCodepoints > node->ownLength)
                fatal("Bad codepoints.\n");
        if (node->totalLength !=
                node->ownLength
                + node_length(get_child(node, RB3_LEFT))
                + node_length(get_child(node, RB3_RIGHT)))
                fatal("Bad lengths.");
        if (node->totalLines !=
                node->ownLines
                + node_lines(get_child(node, RB3_LEFT))
                + node_lines(get_child(node, RB3_RIGHT)))
                fatal("Bad lines.");
        if (node->totalCodepoints !=
                node->ownCodepoints
                + node_codepoints(get_child(node, RB3_LEFT))
                + node_codepoints(get_child(node, RB3_RIGHT)))
                fatal("Bad codepoints.\n");
}

void debug_check_textrope(struct Textrope *rope)
{
        check_node_values(get_root_node(rope));
}

void debug_print_textrope(struct Textrope *rope)
{
        log_begin();
        log_write_cstring("textrope contents: ");
        for (struct Textnode *node = get_leftmost_node(rope);
                node != NULL; node = get_next(node)) {
                log_write(node->text, node->ownLength);
        }
        log_end();
}

void print_textrope_statistics(struct Textrope *rope)
{
        check_node_values(get_root_node(rope));

        FILEPOS nodesVisited = 0;
        FILEPOS bytesUsed = 0;

        for (struct Textnode *node = get_leftmost_node(rope);
             node != NULL; node = get_next(node)) {
                bytesUsed += node->ownLength;
                nodesVisited++;
        }

        log_postf("Textrope: %"FILEPOS_PRI" bytes distributed among %"
                FILEPOS_PRI" nodes. That's %"FILEPOS_PRI" per node",
                bytesUsed, nodesVisited, bytesUsed / nodesVisited);
}
