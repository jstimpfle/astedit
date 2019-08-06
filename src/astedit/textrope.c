#include <rb3ptr.h>
#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/bytes.h>
#include <astedit/logging.h>
#include <astedit/textrope.h>
#include <stddef.h>  // offsetof


static inline int minInt(int a, int b) { return a < b ? a : b; }

static inline int is_utf8_leader_byte(int c) { return (c & 0xc0) != 0x80; }



/* We try to make nodes as big as possible but no bigger than this.
We do this by combining adjacent nodes whose combined size does not exceed
this. This means that each node will always be at least half that size. */
enum {
        TARGET_LENGTH = 3,  /* for testing purposes. Later, switch back to 1024 or so. */
};

struct Textnode {
        struct rb3_head head;
        char *text;
        int ownLength;
        int totalLength;
        /* 0x0a count */
        int ownLines;
        int totalLines;
        /* UTF-8 leader-byte count */
        int ownCodepoints;
        int totalCodepoints;
};

struct Textrope {
        struct rb3_tree tree;
};



static inline struct Textnode *textnode_from_head(struct rb3_head *head)
{
        return (struct Textnode *) (((char *)head) - offsetof(struct Textnode, head));
}


static struct Textnode *create_textnode(void)
{
        log_postf("alloc node!\n");
        // TODO optimization
        struct Textnode *textnode;
        ALLOC_MEMORY(&textnode, 1);
        ALLOC_MEMORY(&textnode->text, TARGET_LENGTH);
        textnode->text[0] = 0;

        ZERO_MEMORY(&textnode->head);
        textnode->ownLength = 0;
        textnode->totalLength = 0;
        textnode->ownLines = 0;
        textnode->totalLines = 0;
        textnode->ownCodepoints = 0;
        textnode->totalCodepoints = 0;

        return textnode;
}

static void destroy_textnode(struct Textnode *textnode)
{
        // TODO optimization
        FREE_MEMORY(&textnode->text);
        FREE_MEMORY(&textnode);
}

struct Textrope *create_textrope(void)
{
        struct Textrope *textrope;
        ALLOC_MEMORY(&textrope, 1);
        rb3_reset_tree(&textrope->tree);
        return textrope;
}

void destroy_textrope(struct Textrope *textrope)
{
        FREE_MEMORY(textrope);
        // TODO destroy all nodes
}





static inline int own_length(struct rb3_head *link)
{
        struct Textnode *node = textnode_from_head(link);
        return node->ownLength;
}

static inline int total_length(struct rb3_head *link)
{
        struct Textnode *node = textnode_from_head(link);
        return node->totalLength;
}

static inline int child_length(struct rb3_head *link, int dir)
{
        struct rb3_head *child = rb3_get_child(link, dir);
        if (child)
                return total_length(child);
        return 0;
}

static inline int own_lines(struct rb3_head *head)
{
        struct Textnode *node = textnode_from_head(head);
        return node->ownLines;
}

static inline int total_lines(struct rb3_head *head)
{
        struct Textnode *node = textnode_from_head(head);
        return node->totalLines;
}

static inline int child_lines(struct rb3_head *head, int dir)
{
        struct rb3_head *child = rb3_get_child(head, dir);
        if (child)
                return total_lines(child);
        return 0;
}

static inline int own_codepoints(struct rb3_head *head)
{
        struct Textnode *node = textnode_from_head(head);
        return node->ownCodepoints;
}

static inline int total_codepoints(struct rb3_head *head)
{
        struct Textnode *node = textnode_from_head(head);
        return node->totalCodepoints;
}

static inline int child_codepoints(struct rb3_head *head, int dir)
{
        struct rb3_head *child = rb3_get_child(head, dir);
        if (child)
                return total_codepoints(child);
        return 0;
}



int textrope_length(struct Textrope *rope)
{
        struct rb3_head *root = rb3_get_root(&rope->tree);
        if (root == NULL)
                return 0;
        return total_length(root);
}

int textrope_number_of_lines(struct Textrope *rope)
{
        struct rb3_head *root = rb3_get_root(&rope->tree);
        if (root == NULL)
                return 0;
        return total_lines(root);
}

int textrope_number_of_lines_quirky(struct Textrope *rope)
{
        int numLines = textrope_number_of_lines(rope);
        int textLength = textrope_length(rope);
        if (textLength > 0) {
                char c;
                copy_text_from_textrope(rope, textLength - 1, &c, 1);
                if (c != '\n')
                        return numLines + 1;
        }
        return numLines;
}

int textrope_number_of_codepoints(struct Textrope *rope)
{
        struct rb3_head *root = rb3_get_root(&rope->tree);
        if (root == NULL)
                return 0;
        return total_codepoints(root);
}





struct Textiter {
        /* reference to node */
        struct rb3_head *current;
        /* parent and childDir together define a place where a reference to a node is stored. */
        struct rb3_head *parent;
        int linkDir;
        /* holds the text position and line number of the first byte of the referenced node's text. */
        int pos;
        int line;
        int codepointPosition;
};

static void init_textiter(struct Textiter *iter, struct Textrope *rope)
{
        //XXX: internal knowledge: root node is linked to the left of base link
        struct rb3_head *current = rb3_get_child(&rope->tree.base, RB3_LEFT);
        iter->current = current;
        iter->parent = &rope->tree.base;
        iter->linkDir = RB3_LEFT;
        if (current == NULL) {
                iter->pos = 0;
                iter->line = 0;
                iter->codepointPosition = 0;
        }
        else {
                iter->pos = child_length(current, RB3_LEFT);
                iter->line = child_lines(current, RB3_LEFT);
                iter->codepointPosition = child_codepoints(current, RB3_LEFT);
        }
}


static void go_to_left_child(struct Textiter *iter)
{
        ENSURE(iter->current != NULL);
        struct rb3_head *child = rb3_get_child(iter->current, RB3_LEFT);
        iter->parent = iter->current;
        iter->current = child;
        iter->linkDir = RB3_LEFT;
        if (child != NULL) {
                struct Textnode *childNode = textnode_from_head(child);
                iter->pos -= child_length(child, RB3_RIGHT);
                iter->pos -= childNode->ownLength;
                iter->line -= child_lines(child, RB3_RIGHT);
                iter->line -= childNode->ownLines;
                iter->codepointPosition -= child_codepoints(child, RB3_RIGHT);
                iter->codepointPosition -= childNode->ownCodepoints;
        }
}

static void go_to_right_child(struct Textiter *iter)
{
        ENSURE(iter->current != NULL);
        struct rb3_head *oldCurrent = iter->current;
        struct rb3_head *child = rb3_get_child(iter->current, RB3_RIGHT);
        iter->parent = oldCurrent;
        iter->current = child;
        iter->linkDir = RB3_RIGHT;
        iter->pos += textnode_from_head(oldCurrent)->ownLength;
        iter->line += textnode_from_head(oldCurrent)->ownLines;
        iter->codepointPosition += textnode_from_head(oldCurrent)->ownCodepoints;
        if (child != NULL) {
                iter->pos += child_length(child, RB3_LEFT);
                iter->line += child_lines(child, RB3_LEFT);
                iter->codepointPosition += child_codepoints(child, RB3_LEFT);
        }
}



static int is_last_line_end(struct rb3_head *head)
{
        struct Textnode *node = textnode_from_head(head);
        ENSURE(node->ownLength > 0);
        return node->text[node->ownLength - 1] == '\n';
}

static int is_first_line_start(struct rb3_head *head)
{
        struct rb3_head *prev = rb3_get_prev(head);
        return prev == NULL || is_last_line_end(prev);
}


static struct Textiter find_first_node_that_contains_the_character_at_pos(struct Textrope *rope, int pos)
{
        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);
        while (iter->current != NULL) {
                if (iter->pos > pos)
                        go_to_left_child(iter);
                else if (iter->pos + own_length(iter->current) <= pos)
                        go_to_right_child(iter);
                else
                        break;
        }
        return textiter;
}

static struct Textiter find_first_node_that_contains_the_given_line(struct Textrope *rope, int line)
{
        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);
        while (iter->current != NULL) {
                if (iter->line > line)
                        go_to_left_child(iter);
                else if (iter->line == line && !is_first_line_start(iter->current))
                        go_to_left_child(iter);
                else if (iter->line + own_lines(iter->current) < line)
                        go_to_right_child(iter);
                else if (iter->line + own_lines(iter->current) == line && is_last_line_end(iter->current))
                        go_to_right_child(iter);
                else
                        break;
        }
        return textiter;
}

static struct Textiter find_first_node_that_contains_the_given_codepointPos(struct Textrope *rope, int codepointPos)
{
        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);
        while (iter->current != NULL) {
                if (iter->codepointPosition > codepointPos)
                        go_to_left_child(iter);
                else if (iter->codepointPosition + own_codepoints(iter->current) <= codepointPos)
                        go_to_right_child(iter);
                else
                        break;
        }
        return textiter;
}




void compute_line_number_and_codepoint_position(
        struct Textrope *rope, int pos, int *outLinenumber, int *outCodepointPosition)
{
        int lineNumber;
        int codepointPosition;

        ENSURE(0 <= pos && pos <= textrope_length(rope));
        if (pos == 0) {
                lineNumber = 0;
                codepointPosition = 0;
        }
        else if (pos == textrope_length(rope)) {
                lineNumber = textrope_number_of_lines(rope);
                codepointPosition = textrope_number_of_codepoints(rope);
        }
        else {
                struct Textiter textiter = find_first_node_that_contains_the_character_at_pos(rope, pos);
                struct Textiter *iter = &textiter;
                ENSURE(iter->current != NULL);
                struct Textnode *node = textnode_from_head(iter->current);
                lineNumber = iter->line;
                codepointPosition = iter->codepointPosition;
                int internalOffset = pos - iter->pos;
                for (int i = 0; i < internalOffset; i++) {
                        if (node->text[i] == '\n')
                                lineNumber++;
                        if (is_utf8_leader_byte(node->text[i]))
                                codepointPosition++;
                }
        }
        *outLinenumber = lineNumber;
        *outCodepointPosition = codepointPosition;
}

int compute_codepoint_position(struct Textrope *rope, int pos)
{
        int lineNumber;
        int codepointPosition;
        compute_line_number_and_codepoint_position(rope, pos, &lineNumber, &codepointPosition);
        return codepointPosition;
}

int compute_line_number(struct Textrope *rope, int pos)
{
        int lineNumber;
        int codepointPosition;
        compute_line_number_and_codepoint_position(rope, pos, &lineNumber, &codepointPosition);
        return lineNumber;
}




int compute_pos_of_line(struct Textrope *rope, int lineNumber)
{
        //XXX special case
        if (lineNumber >= textrope_number_of_lines_quirky(rope))
                return textrope_length(rope);

        struct Textiter textiter = find_first_node_that_contains_the_given_line(rope, lineNumber);
        struct Textiter *iter = &textiter;
        struct Textnode *node = textnode_from_head(iter->current);
        int internalPos = 0;
        int currentLine = iter->line;
        while (currentLine < lineNumber) {
                if (node->text[internalPos] == '\n')
                        currentLine++;
                internalPos++;
        }
        return iter->pos + internalPos;
}

int compute_pos_of_codepoint(struct Textrope *rope, int codepointPos)
{
        struct Textiter textiter = find_first_node_that_contains_the_given_codepointPos(rope, codepointPos);
        struct Textiter *iter = &textiter;
        int currentCodepoints = iter->codepointPosition;
        int internalPos = 0;
        if (iter->current != NULL) {
                struct Textnode *node = textnode_from_head(iter->current);
                while (internalPos < node->ownLength) {
                        if (is_utf8_leader_byte(node->text[internalPos])) {
                                if (currentCodepoints == codepointPos)
                                        break;
                                currentCodepoints++;
                        }
                        internalPos++;
                }
        }
        return iter->pos + internalPos;
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

static void update_textnode_with_child(struct Textnode *node, int linkDir)
{
        struct rb3_head *child = rb3_get_child(&node->head, linkDir);
        if (child != NULL) {
                struct Textnode *childNode = textnode_from_head(child);
                node->totalLength += childNode->totalLength;
                node->totalLines += childNode->totalLines;
                node->totalCodepoints += childNode->totalCodepoints;
        }
}

static void update_textnode(struct Textnode *node)
{
        node->totalLength = node->ownLength;
        node->totalLines = node->ownLines;
        node->totalCodepoints = node->ownCodepoints;
        update_textnode_with_child(node, RB3_LEFT);
        update_textnode_with_child(node, RB3_RIGHT);
}



static void augment_textnode_head(struct rb3_head *head)
{
        struct Textnode *node = textnode_from_head(head);
        update_textnode(node);
}

static void link_textnode_head(struct rb3_head *child, struct rb3_head *parent, int linkDir)
{
        rb3_link_and_rebalance_and_augment(child, parent, linkDir, &augment_textnode_head);
}

static void unlink_textnode_head(struct rb3_head *head)
{
        rb3_unlink_and_rebalance_and_augment(head, &augment_textnode_head);
}

static void link_textnode_head_next_to(struct rb3_head *newNode, struct rb3_head *head, int dir)
{
        if (rb3_has_child(head, dir)) {
                head = rb3_get_prevnext_descendant(head, dir);
                dir = !dir;
        }
        link_textnode_head(newNode, head, dir);
}





struct ChainStream {
        const char *text;
        int readpos;
        int endpos;
        struct ChainStream *next;
};

struct StreamsChain {
        struct ChainStream *streams;
        int remainingBytes;
        int totalLength;
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
                int n = stream->endpos - stream->readpos;
                if (n > todoBytes)
                        n = todoBytes;
                copy_memory(dst, stream->text + stream->readpos, n);
                dst += n;
                todoBytes -= n;
                stream->readpos += n;
                if (stream->readpos == stream->endpos)
                        sc->streams = stream->next;
        }
        sc->remainingBytes -= nbytes;
}


static void copy_from_StreamsChain_to_Textnode(struct StreamsChain *sc, struct Textnode *node, int offset, int length)
{
        //XXX
        if (length > sc->remainingBytes)
                length = sc->remainingBytes;
        copy_from_StreamsChain(sc, node->text + offset, length);
        added_range(node, node->text + offset, length);
        node->ownLength += length;
}







void insert_text_into_textrope(struct Textrope *rope, int pos, const char *text, int length)
{
        ENSURE(pos <= textrope_length(rope));

        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);

        /* if the tree is currently empty, make a node with content length 0. */
        if (iter->current == NULL) {
                ENSURE(pos == 0);
                struct Textnode *node = create_textnode();
                link_textnode_head(&node->head, iter->parent, iter->linkDir);
                init_textiter(iter, rope);
        }

        /* find the node that either properly contains the offset pos or whose
        right border is at pos. */
        for (;;) {
                if (iter->pos > pos)
                        go_to_left_child(iter);
                else if (iter->pos + own_length(iter->current) < pos)
                        go_to_right_child(iter);
                else
                        break;
        }

        ENSURE(iter->current != NULL);

        struct rb3_head *head = iter->current;
        struct Textnode *node = textnode_from_head(head);
        int internalPos = pos - iter->pos;
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
        stream2.endpos = length;
        stream2.next = NULL;

        struct StreamsChain chain;
        init_StreamsChain(&chain);
        append_to_StreamsChain(&chain, &stream1);
        append_to_StreamsChain(&chain, &stream2);

        int finalBytesAvailable;
        {
                struct rb3_head *succ = rb3_get_next(head);
                finalBytesAvailable = succ == NULL ? 0 : TARGET_LENGTH - own_length(succ);
        }

        copy_from_StreamsChain_to_Textnode(&chain, node, node->ownLength, TARGET_LENGTH - node->ownLength);
        rb3_update_augment(head, &augment_textnode_head);

        while (chain.remainingBytes > finalBytesAvailable) {
                struct rb3_head *lastHead = head;
                node = create_textnode();
                head = &node->head;
                copy_from_StreamsChain_to_Textnode(&chain, node, node->ownLength, TARGET_LENGTH - node->ownLength);
                link_textnode_head_next_to(head, lastHead, RB3_RIGHT);
        }

        if (chain.remainingBytes > 0) {
                ENSURE(chain.remainingBytes <= finalBytesAvailable);
                head = rb3_get_next(head);
                node = textnode_from_head(head);
                ENSURE(TARGET_LENGTH - node->ownLength == finalBytesAvailable);
                move_memory(node->text, chain.remainingBytes, node->ownLength);
                copy_from_StreamsChain_to_Textnode(&chain, node, 0, chain.remainingBytes);
                rb3_update_augment(head, &augment_textnode_head);
        }
}

void erase_text_from_textrope(struct Textrope *rope, int pos, int length)
{
        ENSURE(0 <= pos);
        ENSURE(pos + length <= textrope_length(rope));

        struct Textiter textiter = find_first_node_that_contains_the_character_at_pos(rope, pos);
        struct Textiter *iter = &textiter;

        struct rb3_head *head = iter->current;
        int numDeleted = 0;

        /* partial delete from left node of range */
        if (pos != iter->pos) {
                struct Textnode *node = textnode_from_head(head);
                int internalPos = pos - iter->pos;
                ENSURE(0 <= internalPos && internalPos < node->ownLength);
                int nd = length - numDeleted;
                if (nd > node->ownLength - internalPos)
                        nd = node->ownLength - internalPos;

                erased_range(node, node->text + internalPos, nd);
                move_memory(node->text + internalPos + nd, -nd,
                        node->ownLength - internalPos - nd);
                numDeleted += nd;
                node->ownLength -= nd;
                /* We might optimize this augmentation away:
                Maybe we need to augment this node again later anyway. */
                rb3_update_augment(head, &augment_textnode_head);

                /* move to next node. */
                head = rb3_get_next(head);
        }

        /* Unlink/delete all nodes whose contents are completely in the
        to-be-deleted range. */
        while (length != numDeleted) {
                if (head == NULL) {
                        log_postf("AAAH!\n");
                }
                ENSURE(head != NULL);

                struct Textnode *node = textnode_from_head(head);

                if (node->ownLength > length - numDeleted)
                        break;

                numDeleted += node->ownLength;

                struct rb3_head *nextHead = rb3_get_next(head);
                unlink_textnode_head(head);
                destroy_textnode(node);
                head = nextHead;
        }

        /* partial delete from last node */
        if (length > numDeleted) {
                ENSURE(head != NULL);
                struct Textnode *node = textnode_from_head(head);
                int nd = length - numDeleted;
                ENSURE(numDeleted + node->ownLength >= nd);
                /* Just erase the last chunk of text inside the last node */
                erased_range(node, node->text, nd);
                move_memory(node->text + nd, -nd, node->ownLength - nd);
                node->ownLength -= nd;
                rb3_update_augment(head, &augment_textnode_head);
        }

        /* maybe merge last visited node with previous */
        if (head != NULL) {
                struct Textnode *node = textnode_from_head(head);
                /* this may or may not be (depending on if we did a partial erase)
                the first node that we visited */
                struct rb3_head *prevHead = rb3_get_prev(head);
                if (prevHead != NULL) {
                        struct Textnode *prevNode = textnode_from_head(prevHead);
                        if (prevNode->ownLength + node->ownLength <= TARGET_LENGTH) {
                                copy_memory(prevNode->text + prevNode->ownLength,
                                        node->text, node->ownLength);
                                added_range(prevNode, node->text, node->ownLength);
                                prevNode->ownLength += node->ownLength;
                                rb3_update_augment(prevHead, &augment_textnode_head);
                                unlink_textnode_head(head);
                                destroy_textnode(node);
                        }
                }
        }
}

int copy_text_from_textrope(struct Textrope *rope, int offset, char *dstBuffer, int length)
{
        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);

        while (iter->current) {
                if (iter->pos > offset)
                        go_to_left_child(iter);
                else if (iter->pos + own_length(iter->current) <= offset)
                        go_to_right_child(iter);
                else
                        break;
        }

        int numRead = 0;

        struct rb3_head *head = iter->current;
        if (head != NULL) {
                /* Read from first node - we might have to start somewhere in the middle */
                struct Textnode *node = textnode_from_head(head);
                int internalPos = offset - iter->pos;
                ENSURE(0 <= internalPos && internalPos < node->ownLength);
                int numToRead = minInt(length, node->ownLength - internalPos);
                copy_memory(dstBuffer, node->text + internalPos, numToRead);
                numRead += numToRead;
                head = rb3_get_next(head);
        }

        /* Read subsequent nodes */
        while (head != NULL && numRead < length) {
                struct Textnode *node = textnode_from_head(head);
                int numToRead = minInt(length - numRead, node->ownLength);
                copy_memory(dstBuffer + numRead, node->text, numToRead);
                numRead += numToRead;
                head = rb3_get_next(head);
        }

        return numRead;
}













static void check_node_values(struct rb3_head *head)
{
        if (head == NULL)
                return;
        check_node_values(rb3_get_child(head, RB3_LEFT));
        check_node_values(rb3_get_child(head, RB3_RIGHT));
        struct Textnode *node = textnode_from_head(head);

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
                + child_length(head, RB3_LEFT)
                + child_length(head, RB3_RIGHT)) {
                fatal("Bad lengths.");
        }
        if (node->totalLines !=
                node->ownLines
                + child_lines(head, RB3_LEFT)
                + child_lines(head, RB3_RIGHT)) {
                fatal("Bad lines.");
        }
        if (node->totalCodepoints !=
                node->ownCodepoints
                + child_codepoints(head, RB3_LEFT)
                + child_codepoints(head, RB3_RIGHT)) {
                fatal("Bad codepoints.\n");
        }
}

void debug_check_textrope(struct Textrope *rope)
{
        check_node_values(rb3_get_root(&rope->tree));
}

void debug_print_textrope(struct Textrope *rope)
{
        log_begin();
        log_write_cstring("textrope contents: ");
        for (struct rb3_head *head = rb3_get_min(&rope->tree);
                head != NULL; head = rb3_get_next(head)) {
                struct Textnode *node = textnode_from_head(head);
                log_write(node->text, node->ownLength);
                head = rb3_get_next(head);
        }
        log_end();
}

void print_textrope_statistics(struct Textrope *rope)
{
        check_node_values(rb3_get_root(&rope->tree)); //XXX

        int nodesVisited = 0;
        int bytesUsed = 0;

        struct rb3_head *head = rb3_get_min(&rope->tree);
        while (head != NULL) {
                struct Textnode *node = textnode_from_head(head);
                bytesUsed += node->ownLength;
                nodesVisited++;
                head = rb3_get_next(head);
        }

        log_postf("Textrope: %d bytes distributed among %d nodes. That's %d per node",
                bytesUsed, nodesVisited, bytesUsed / nodesVisited);
}
