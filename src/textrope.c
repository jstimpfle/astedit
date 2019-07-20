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


static int next_power_of_2(int x)
{
        ENSURE(x > 0);
        x = x * 2 - 1;
        while (x & (x - 1))
                x = x & (x - 1);
        return x;
}


/* We try to make nodes as big as possible but no bigger than this.
We do this by combining adjacent nodes whose combined size does not exceed
this. This means that each node will always be at least half that size. */
enum {
        TARGET_WEIGHT = 1024,
};


struct Textnode {
        struct rb3_head head;
        char *text;
        int textCapacity;
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





static void realloc_node_text(struct Textnode *textnode, int newLength)
{
        // add one more to be able to zero terminate which is nicer for debugging
        if (textnode->textCapacity < newLength + 1
            || newLength + 1 <= textnode->textCapacity) {
                int newCap = next_power_of_2(newLength + 1);
                ENSURE(newCap >= newLength + 1);
                REALLOC_MEMORY(&textnode->text, newCap);
                textnode->textCapacity = newCap;
        }
        textnode->text[newLength] = '\0';
        textnode->ownLength = newLength;
        // NOTE we don't call the augmentation function here. The node might not be linked!
}



static struct Textnode *create_textnode(const char *text, int length)
{
        // TODO optimization
        struct Textnode *textnode;
        ALLOC_MEMORY(&textnode, 1);

        ZERO_MEMORY(&textnode->head);

        textnode->text = NULL;
        textnode->textCapacity = 0;
        realloc_node_text(textnode, length);

        COPY_ARRAY(textnode->text, text, length);
        textnode->totalLength = length;

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

void init_textiter(struct Textiter *iter, struct Textrope *rope)
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

/* returns if iterator is currently at root. More precisely, if iter->parent
is the base link. That means that even if iter->current == NULL, the iterator
could still be "at root" */
int is_at_root(struct Textiter *iter)
{
        return rb3_is_base(iter->parent);
}

void go_to_parent(struct Textiter *iter)
{
        ENSURE(!is_at_root(iter));
        if (iter->linkDir == RB3_LEFT) {
                if (iter->current) {
                        iter->pos += own_length(iter->current);
                        iter->pos += child_length(iter->current, RB3_RIGHT);
                }
        }
        else {
                if (iter->current) {
                        iter->pos -= child_length(iter->current, RB3_LEFT);
                }
                iter->pos -= own_length(iter->parent);
        }
        iter->current = iter->parent;
        iter->parent = rb3_get_parent(iter->current);
        iter->linkDir = rb3_get_parent_dir(iter->current);
}

void go_to_left_child(struct Textiter *iter)
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

void go_to_right_child(struct Textiter *iter)
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

void go_to_leaf_left_null(struct Textiter *iter)
{
        ENSURE(iter->current != NULL);
        go_to_left_child(iter);
        while (iter->current != NULL)
                go_to_right_child(iter);
}

void go_to_leaf_right_null(struct Textiter *iter)
{
        ENSURE(iter->current != NULL);
        go_to_right_child(iter);
        while (iter->current != NULL)
                go_to_left_child(iter);
}





static struct Textiter find_topmost_that_contains_or_touches_offset(struct Textrope *rope, int offset)
{
        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);
        while (iter->current) {
                if (iter->pos > offset)
                        go_to_left_child(iter);
                else if (iter->pos + own_length(iter->current) < offset)
                        go_to_right_child(iter);
                else
                        break;
        }
        return textiter;
}

static struct Textiter find_first_that_contains_character(struct Textrope *rope, int pos)
{
        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);
        while (iter->current) {
                if (iter->pos > pos)
                        go_to_left_child(iter);
                else if (iter->pos + own_length(iter->current) <= pos)
                        go_to_right_child(iter);
                else
                        break;
        }
        return textiter;
}





static void augment_textnode_head(struct rb3_head *head)
{
        struct Textnode *node = textnode_from_head(head);
        node->totalLength = own_length(head)
                + child_length(head, RB3_LEFT)
                + child_length(head, RB3_RIGHT);
}

static void link_textnode_head(struct rb3_head *child, struct rb3_head *parent, int linkDir)
{
        rb3_link_and_rebalance_and_maybe_augment(child, parent, linkDir, &augment_textnode_head);
}

static void unlink_textnode_head(struct rb3_head *head)
{
        rb3_unlink_and_rebalance_and_maybe_augment(head, &augment_textnode_head);
}



void insert_text_into_textrope(struct Textrope *rope, int pos, const char *text, int length)
{
        struct Textiter textiter = find_topmost_that_contains_or_touches_offset(rope, pos);
        struct Textiter *iter = &textiter;

        if (iter->current == NULL) {
                struct Textnode *newNode = create_textnode(text, length);
                link_textnode_head(&newNode->head, iter->parent, iter->linkDir);
                return;
        }

        /* currently visited node contains pos */
        int rangeStart = iter->pos;
        int rangeEnd = iter->pos + (iter->current ? own_length(iter->current) : 0);
        ENSURE(rangeStart <= pos && pos <= rangeEnd);


        if (total_length(iter->current) + length <= TARGET_WEIGHT) {
                struct rb3_head *head = iter->current;
                struct Textnode *node = textnode_from_head(head);
                int internalPos = pos - rangeStart;
                int moveLength = node->ownLength - internalPos;
                realloc_node_text(node, node->ownLength + length);
                move_memory(node->text + internalPos, length, moveLength);
                copy_memory(node->text + internalPos, text, length);
                rb3_update_augment(head, &augment_textnode_head);
        }
        else if (rangeStart == pos || rangeEnd == pos) {
                if (rangeStart == pos)
                        go_to_leaf_left_null(iter);
                else
                        go_to_leaf_right_null(iter);
                struct Textnode *newNode = create_textnode(text, length);
                link_textnode_head(&newNode->head, iter->parent, iter->linkDir);
        }
        else {
                struct rb3_head *old = iter->current;
                struct Textnode *oldNode = textnode_from_head(old);
                int d = pos - rangeStart;
                unlink_textnode_head(old);
                insert_text_into_textrope(rope, rangeStart, oldNode->text + d, oldNode->ownLength - d);
                insert_text_into_textrope(rope, rangeStart, text, length);
                insert_text_into_textrope(rope, rangeStart, oldNode->text, d);
                destroy_textnode(oldNode);
        }
}



void erase_text_from_textrope(struct Textrope *rope, int pos, int length)
{
        ENSURE(textrope_length(rope) >= pos + length);

        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);

        /* find start of to-be-deleted range */
        while (iter->current != NULL) {
                if (iter->pos > pos)
                        go_to_left_child(iter);
                else if (iter->pos + own_length(iter->current) < pos)
                        go_to_right_child(iter);
                else
                        break;
        }
        ENSURE(iter->current != NULL);  // that would mean: pos out of range

        struct rb3_head *head = iter->current;
        int headpos = iter->pos;
        int numBytesErased = 0;
        //log_postf("headpos, pos, length: %d, %d, %d\n", headpos, pos, length);
        while (head && headpos < pos + length) {
                struct Textnode *node = textnode_from_head(head);
                struct rb3_head *next = rb3_get_next(head);
                int nextpos = headpos + node->ownLength;
                int erasedHere;
                if (headpos >= pos) {
                        erasedHere = length - numBytesErased;
                        if (erasedHere < node->ownLength) {
                                //log_postf("HERE!\n");
                                move_memory(node->text + erasedHere, -erasedHere,
                                            node->ownLength - erasedHere);
                                realloc_node_text(node, node->ownLength - erasedHere);
                                rb3_update_augment(head, &augment_textnode_head);
                        }
                        else {
                                //log_postf("DESTROY\n");
                                erasedHere = node->ownLength;
                                unlink_textnode_head(head);
                                destroy_textnode(node);
                        }
                }
                else {
                        //log_postf("delete middle or end\n");
                        ENSURE(headpos < pos);
                        int internalOffset = pos - headpos;
                        erasedHere = length - numBytesErased;
                        if (erasedHere > node->ownLength - internalOffset)
                                erasedHere = node->ownLength - internalOffset;
                        realloc_node_text(node, node->ownLength - erasedHere);
                        rb3_update_augment(head, &augment_textnode_head);
                }
                head = next;
                headpos = nextpos;
        }

        // TODO: merge first and last nodes?
}

int copy_text_from_textrope(struct Textrope *rope, int offset, char *dstBuffer, int length)
{
        struct Textiter textiter = find_first_that_contains_character(rope, offset);
        struct Textiter *iter = &textiter;        

        int numRead = 0;

        struct rb3_head *head = iter->current;
        if (head != NULL) {
                /* read from first node - we might have to start somewhere in the middle */
                struct Textnode *node = textnode_from_head(head);
                int internalPos = offset - iter->pos;
                ENSURE(0 <= internalPos && internalPos < node->ownLength);
                int numToRead = minInt(length, node->ownLength - internalPos);
                copy_memory(dstBuffer, node->text + internalPos, numToRead);
                numRead += numToRead;
                head = rb3_get_next(head);
        }

        /* read subsequent nodes */
        while (head != NULL && numRead < length) {
                struct Textnode *node = textnode_from_head(head);
                int numToRead = minInt(length - numRead, node->ownLength);
                copy_memory(dstBuffer + numRead, node->text, numToRead);
                numRead += numToRead;
                head = rb3_get_next(head);
        }

        return numRead;
}
