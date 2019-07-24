#include <rb3ptr.h>
#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/bytes.h>
#include <astedit/logging.h>
#include <astedit/textrope.h>
#include <stddef.h>  // offsetof


static int minInt(int a, int b) {
        return a < b ? a : b;
}



/* We try to make nodes as big as possible but no bigger than this.
We do this by combining adjacent nodes whose combined size does not exceed
this. This means that each node will always be at least half that size. */
enum {
        TARGET_LENGTH = 1024,  /* for testing purposes. Later, switch back to 1024 or so. */
};

struct Textnode {
        struct rb3_head head;
        char *text;
        int ownLength;
        int totalLength;
};

struct Textrope {
        struct rb3_tree tree;
};




static inline struct Textnode *textnode_from_head(struct rb3_head *head)
{
        return (struct Textnode *) (((char *)head) - offsetof(struct Textnode, head));
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

int textrope_length(struct Textrope *rope)
{
        struct rb3_head *root = rb3_get_root(&rope->tree);
        if (root == NULL)
                return 0;
        return total_length(root);
}






void check_node_values(struct rb3_head *head)
{
        if (head == NULL)
                return;
        check_node_values(rb3_get_child(head, RB3_LEFT));
        check_node_values(rb3_get_child(head, RB3_RIGHT));
        struct Textnode *node = textnode_from_head(head);
        if (node->totalLength !=
                node->ownLength
                + child_length(head, RB3_LEFT)
                + child_length(head, RB3_RIGHT)) {
                fatal("Bad weights.");
        }
}

void debug_print_textrope(struct Textrope *rope)
{
       check_node_values(rb3_get_root(&rope->tree)); //XXX

        struct rb3_head *head = rb3_get_min(&rope->tree);
        log_begin();
        log_write_cstring("textrope contents: ");
        while (head != NULL) {
                struct Textnode *node = textnode_from_head(head);
                log_write(node->text, node->ownLength);

                head = rb3_get_next(head);
        }
        log_end();
}

void print_textrope_statistics(struct Textrope *rope)
{
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



static struct Textnode *create_textnode(void)
{
        // TODO optimization
        struct Textnode *textnode;
        ALLOC_MEMORY(&textnode, 1);
        ALLOC_MEMORY(&textnode->text, TARGET_LENGTH);
        textnode->text[0] = 0;

        ZERO_MEMORY(&textnode->head);
        textnode->ownLength = 0;
        textnode->totalLength = 0;

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



struct Textiter {
        /* reference to node */
        struct rb3_head *current;
        /* parent and childDir together define a place where a reference to a node is stored. */
        struct rb3_head *parent;
        int linkDir;
        /* holds the start position in text of the referenced node. */
        int pos;
};

static void init_textiter(struct Textiter *iter, struct Textrope *rope)
{
        //XXX: internal knowledge: root node is linked to the left of base link
        struct rb3_head *current = rb3_get_child(&rope->tree.base, RB3_LEFT);
        iter->current = current;
        iter->parent = &rope->tree.base;
        iter->linkDir = RB3_LEFT;
        if (current == NULL)
                iter->pos = 0;
        else
                iter->pos = child_length(current, RB3_LEFT);
}


static void go_to_left_child(struct Textiter *iter)
{
        ENSURE(iter->current != NULL);
        struct rb3_head *child = rb3_get_child(iter->current, RB3_LEFT);
        iter->parent = iter->current;
        iter->current = child;
        iter->linkDir = RB3_LEFT;
        if (child != NULL) {
                iter->pos -= child_length(child, RB3_RIGHT);
                iter->pos -= textnode_from_head(child)->ownLength;
        }
}

static void go_to_right_child(struct Textiter *iter)
{
        ENSURE(iter->current != NULL);
        struct rb3_head *oldCurrent = iter->current;
        struct rb3_head *child = rb3_get_child(iter->current, RB3_RIGHT);
        iter->parent = iter->current;
        iter->current = child;
        iter->linkDir = RB3_RIGHT;
        iter->pos += textnode_from_head(oldCurrent)->ownLength;
        if (child != NULL)
                iter->pos += child_length(child, RB3_LEFT);
}





static void augment_textnode_head(struct rb3_head *head)
{
        struct Textnode *node = textnode_from_head(head);
        node->totalLength = node->ownLength
                + child_length(head, RB3_LEFT)
                + child_length(head, RB3_RIGHT);
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



void insert_text_into_textrope(struct Textrope *rope, int pos, const char *text, int length)
{
        ENSURE(pos <= textrope_length(rope));

        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);

        /* if the tree is currently empty, make an node with content length 0. */
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

        /*
        If the found node can hold all the new text, we'll just insert it there.
        */
        if (node->ownLength + length <= TARGET_LENGTH) {
                move_memory(node->text + internalPos, length, node->ownLength - internalPos);
                copy_memory(node->text + internalPos, text, length);
                node->ownLength += length;
                rb3_update_augment(&node->head, &augment_textnode_head);
                return;
        }

        /*
        Otherwise, we need to tuck away the data in that position.
        */
        char tuck[TARGET_LENGTH];
        int tuckbytes = node->ownLength - internalPos;
        ENSURE(0 <= tuckbytes && tuckbytes <= TARGET_LENGTH);
        copy_memory(&tuck[0], node->text + internalPos, tuckbytes);
        node->ownLength = internalPos;

        /* position of remaining text to insert */
        int readpos = 0;

        /* we can use the freed space to store some of the data already */
        {
                int nw = TARGET_LENGTH - internalPos;
                if (nw > length - readpos)
                        nw = length - readpos;
                copy_memory(node->text + internalPos, text + readpos, nw);
                readpos += nw;
                node->ownLength += nw;
        }

        /*
        Now we look at the next (successor) node to store more data.
        */
        struct Textnode *succNode;
        struct rb3_head *succ = rb3_get_next(head);
        if (succ != NULL)
                succNode = textnode_from_head(succ);
        else {
                /* If there is no successor node, we create one. */
                succNode = create_textnode();
                succ = &succNode->head;
                link_textnode_head_next_to(succ, head, RB3_RIGHT);
        }

        /*
        We use the successor node to potentially hold the remaining data.
        Whenever the free space there isn't sufficient to store all of it,
        we create a new node just before it.
        */
        while (readpos < length
                && (TARGET_LENGTH - succNode->ownLength < length - readpos + tuckbytes)) {
                /* Create new node before (previous) successor. */
                node = create_textnode();
                head = &node->head;

                int nw = TARGET_LENGTH;
                if (nw > length - readpos)
                        nw = length - readpos;
                copy_memory(node->text, text + readpos, nw);
                readpos += nw;
                node->ownLength = nw;

                link_textnode_head_next_to(head, succ, RB3_LEFT);
        }

        if (readpos < length) {
                /* we can store everything in the successor node. */
                move_memory(succNode->text, length - readpos + tuckbytes, succNode->ownLength);
                copy_memory(succNode->text, text + readpos, length - readpos);
                copy_memory(succNode->text + length - readpos, tuck, tuckbytes);
                succNode->ownLength += length - readpos + tuckbytes;
                readpos += length - readpos + tuckbytes;
                rb3_update_augment(&succNode->head, &augment_textnode_head);
        }
        else {
                /* we only need to store the tuckbytes. TODO: optimize this by
                filling the current node first (then maybe we don't have to
                create a new node). */
                node = create_textnode();
                head = &node->head;
                copy_memory(node->text, tuck, tuckbytes);
                node->ownLength = tuckbytes;
                link_textnode_head_next_to(head, succ, RB3_LEFT);
        }
}



void erase_text_from_textrope(struct Textrope *rope, int pos, int length)
{
        ENSURE(0 <= pos);
        ENSURE(pos + length <= textrope_length(rope));

        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);

        /* find first node that contains the character at pos. */
        while (iter->current != NULL) {
                if (iter->pos > pos)
                        go_to_left_child(iter);
                else if (iter->pos + own_length(iter->current) <= pos)
                        go_to_right_child(iter);
                else
                        break;
        }

        struct rb3_head *firstHead = iter->current;
        struct Textnode *firstNode = textnode_from_head(firstHead);
        int internalPos = pos - iter->pos;
        ENSURE(0 <= internalPos && internalPos < firstNode->ownLength);

        int numDeleted = 0;
        /* delete from the first head with offset */
        {
                int nd = length - numDeleted;
                if (nd > firstNode->ownLength - internalPos)
                        nd = firstNode->ownLength - internalPos;
                
                move_memory(firstNode->text + internalPos + nd, -nd,
                        firstNode->ownLength - internalPos - nd);
                numDeleted += nd;
                firstNode->ownLength -= nd;
                /* We might optimize this augmentation away:
                Maybe we need to augment this node again later anyway. */
                rb3_update_augment(firstHead, &augment_textnode_head);
        }

        struct rb3_head *head = firstHead;
        struct Textnode *node = firstNode;

        /* Unlink/delete all nodes whose contents are completely in the
        to-be-deleted range. */
        for (;;) {
                head = rb3_get_next(head);
                node = textnode_from_head(head);

                if (numDeleted == length)
                        break;

                int nd = length - numDeleted;
                if (nd < node->ownLength)
                        break;

                unlink_textnode_head(head);
                destroy_textnode(node);
        }

        /* delete text from last node */
        int nd = length - numDeleted;
        ENSURE(nd == 0 || node != NULL);
        if (node != NULL && firstNode->ownLength + node->ownLength - nd <= TARGET_LENGTH) {
                /* We can move remaining text from last node to previous node.
                Then we can delete the last node */
                ENSURE(nd <= node->ownLength);
                copy_memory(firstNode->text + firstNode->ownLength,
                        node->text + nd, node->ownLength - nd);
                firstNode->ownLength += node->ownLength - nd;
                unlink_textnode_head(head);
                destroy_textnode(node);
                rb3_update_augment(firstHead, &augment_textnode_head);
        }
        else if (nd > 0) {
                /* Just erase the last chunk of text inside the last node */
                move_memory(node->text + nd, -nd, node->ownLength - nd);
                node->ownLength -= nd;
                rb3_update_augment(head, &augment_textnode_head);
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
