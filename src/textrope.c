#include <rb3ptr.h>
#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/bytes.h>
#include <astedit/logging.h>
#include <astedit/textrope.h>
#include <stddef.h>  // offsetof


struct Textnode {
        struct rb3_head link;
        char *text;
        int ownWeight;
        int totalWeight;
};

struct Textrope {
        struct rb3_tree tree;
};

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


static struct Textnode *create_textnode(const char *text, int length)
{
        // TODO optimization
        struct Textnode *textnode;
        ALLOC_MEMORY(&textnode, 1);
        ALLOC_MEMORY(&textnode->text, length + 1);
        COPY_ARRAY(textnode->text, text, length);
        textnode->text[length] = '\0';
        textnode->ownWeight = length;
        textnode->totalWeight = length;
        return textnode;
}

static void destroy_textnode(struct Textnode *textnode)
{
        // TODO optimization
        FREE_MEMORY(&textnode->text);
        FREE_MEMORY(&textnode);
}


static inline struct Textnode *textnode_from_head(struct rb3_head *head)
{
        return (struct Textnode *) (((char *)head) - offsetof(struct Textnode, link));
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


int node_weight(struct rb3_head *link)
{
        if (link == NULL)
                return 0;
        struct Textnode *node = textnode_from_head(link);
        return node->totalWeight;
}

static inline int own_weight(struct rb3_head *link)
{
        struct Textnode *node = textnode_from_head(link);
        return node->ownWeight;
}

static inline int total_weight(struct rb3_head *link)
{
        struct Textnode *node = textnode_from_head(link);
        return node->totalWeight;
}

static inline int child_weight(struct rb3_head *link, int dir)
{
        struct rb3_head *child = rb3_get_child(link, dir);
        return node_weight(child);
}



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
                iter->pos = child_weight(current, RB3_LEFT);
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
                        iter->pos += own_weight(iter->current);
                        iter->pos += child_weight(iter->current, RB3_RIGHT);
                }
        }
        else {
                if (iter->current) {
                        iter->pos -= child_weight(iter->current, RB3_LEFT);
                }
                iter->pos -= own_weight(iter->parent);
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
                iter->pos -= child_weight(child, RB3_RIGHT);
                iter->pos -= textnode_from_head(child)->ownWeight;
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
        iter->pos += textnode_from_head(oldCurrent)->ownWeight;
        if (child != NULL)
                iter->pos += child_weight(child, RB3_LEFT);
}

static void augment_textnode_head(struct rb3_head *head)
{
        struct Textnode *node = textnode_from_head(head);
        node->totalWeight = own_weight(head)
                + child_weight(head, RB3_LEFT)
                + child_weight(head, RB3_RIGHT);
}

static void link_textnode_head(struct rb3_head *child, struct rb3_head *parent, int linkDir)
{
        rb3_link_and_rebalance_and_maybe_augment(child, parent, linkDir, &augment_textnode_head);
}

static void unlink_textnode_head(struct rb3_head *head)
{
        rb3_unlink_and_rebalance_and_maybe_augment(head, &augment_textnode_head);
}



void check_node_values(struct rb3_head *head)
{
        if (head == NULL)
                return;
        check_node_values(rb3_get_child(head, RB3_LEFT));
        check_node_values(rb3_get_child(head, RB3_RIGHT));
        struct Textnode *node = textnode_from_head(head);
        if (node->totalWeight !=
                node->ownWeight
                + child_weight(head, RB3_LEFT)
                + child_weight(head, RB3_RIGHT)) {
                ENSURE(0 && "Bad weights.");
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
                log_write(node->text, node->ownWeight);
                head = rb3_get_next(head);
        }
        log_end();
}

static struct Textiter find_first_that_contains_position(struct Textrope *rope, int pos)
{
        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);
        while (iter->current) {
                if (iter->pos > pos)
                        go_to_left_child(iter);
                else if (iter->pos + own_weight(iter->current) <= pos)
                        go_to_right_child(iter);
                else
                        break;
        }
        return textiter;
}

static struct Textiter findpos(struct Textrope *rope, int pos)
{
        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);
        while (iter->current) {
                if (iter->pos >= pos)
                        go_to_left_child(iter);
                else if (iter->pos + own_weight(iter->current) <= pos)
                        go_to_right_child(iter);
                else
                        /* in range */
                        return textiter;
        }
        if (iter->pos == pos)
                return textiter;
        /* out of range. What to do? */
        UNREACHABLE();
}


static struct Textiter split(struct Textrope *rope, int pos)
{
        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);
        while (iter->current) {
                if (iter->pos > pos)
                        go_to_left_child(iter);
                else if (iter->pos + own_weight(iter->current) <= pos)
                        go_to_right_child(iter);
                else
                        break;
        }
        if (iter->pos == pos)
                return textiter;  /* already split, or pos == 0, pos == total length */
        if (iter->current == NULL)
                /* out of range. What to do? */
                ENSURE(0);
        /* split */
        struct rb3_head *old = iter->current;
        struct Textnode *oldNode = textnode_from_head(old);
        int internalPos = pos - iter->pos;
        ENSURE(0 < internalPos && internalPos < oldNode->ownWeight);
        unlink_textnode_head(old);
        insert_text_into_textrope(rope, iter->pos, oldNode->text + internalPos, oldNode->ownWeight - internalPos);
        insert_text_into_textrope(rope, iter->pos, oldNode->text, internalPos);
        /*
        ENSURE(rb3_check_tree(&rope->tree)); //XXX
        check_node_values(rb3_get_root(&rope->tree)); //XXX
        */
        destroy_textnode(oldNode);

        // TODO: optimize this away. We should have all the relevant information already, no need to start another search.
        return findpos(rope, pos);
}


void insert_text_into_textrope(struct Textrope *rope, int pos, const char *text, int length)
{
        struct Textiter textiter = findpos(rope, pos);
        struct Textiter *iter = &textiter;

        /* currently visited node contains pos */
        int rangeStart = iter->pos;
        int rangeEnd = iter->pos + (iter->current ? own_weight(iter->current) : 0);
        ENSURE(rangeStart <= pos && pos <= rangeEnd);

        if (rangeStart < pos && pos < rangeEnd) {
                ENSURE(iter->current != NULL);
                /* If the position lies properly inside the range,
                we need to split the currently visited node, and start
                the search again. */
                struct rb3_head *old = iter->current;
                struct Textnode *oldNode = textnode_from_head(old);
                int d = pos - rangeStart;
                unlink_textnode_head(old);
                insert_text_into_textrope(rope, rangeStart, oldNode->text + d, oldNode->ownWeight - d);
                insert_text_into_textrope(rope, rangeStart, text, length);
                insert_text_into_textrope(rope, rangeStart, oldNode->text, d);
                destroy_textnode(oldNode);
        }
        else {
                /* Otherwise (if the position lies on one of the ends
                of the range) we can insert the new text here. */
                ENSURE(iter->current == NULL);
                struct Textnode *newNode = create_textnode(text, length);
                link_textnode_head(&newNode->link, iter->parent, iter->linkDir);
        }
}

int textrope_length(struct Textrope *rope)
{
        struct rb3_head *root = rb3_get_root(&rope->tree);
        if (root == NULL)
                return 0;
        return total_weight(root);
}


void erase_text_from_textrope(struct Textrope *rope, int pos, int length)
{
        ENSURE(textrope_length(rope) >= pos + length);

        split(rope, pos);
        split(rope, pos + length);

        struct Textiter iter = find_first_that_contains_position(rope, pos);
        struct Textiter end = find_first_that_contains_position(rope, pos + length);
        struct rb3_head *head = iter.current;
        while (head != end.current) {
                struct rb3_head *tmp = rb3_get_next(head);
                unlink_textnode_head(head);
                destroy_textnode(textnode_from_head(head));
                head = tmp;
        }
}

static int minInt(int a, int b) {
        return a < b ? a : b;
}

int copy_text_from_textrope(struct Textrope *rope, int offset, char *dstBuffer, int length)
{
        struct Textiter textiter = find_first_that_contains_position(rope, offset);
        struct Textiter *iter = &textiter;        

        int numRead = 0;

        struct rb3_head *head = iter->current;
        if (head != NULL) {
                /* read from first node - we might have to start somewhere in the middle */
                struct Textnode *node = textnode_from_head(head);
                int internalPos = offset - iter->pos;
                ENSURE(0 <= internalPos && internalPos < node->ownWeight);
                int numToRead = minInt(length, node->ownWeight - internalPos);
                COPY_ARRAY(dstBuffer, node->text + internalPos, numToRead);
                numRead += numToRead;
                head = rb3_get_next(head);
        }

        /* read subsequent nodes */
        while (head != NULL && numRead < length) {
                struct Textnode *node = textnode_from_head(head);
                int numToRead = minInt(length - numRead, node->ownWeight);
                COPY_ARRAY(dstBuffer + numRead, node->text, numToRead);
                numRead += numToRead;
                head = rb3_get_next(head);
        }
        return numRead;
}