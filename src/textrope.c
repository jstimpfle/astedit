#include <rb3ptr.h>
#include <astedit/astedit.h>
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


static struct Textnode *alloc_textnode(const char *text, int length)
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



static struct Textnode *textnode_from_head(struct rb3_head *head)
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

int own_weight(struct rb3_head *link)
{
        struct Textnode *node = textnode_from_head(link);
        return node->ownWeight;
}

int total_weight(struct rb3_head *link)
{
        struct Textnode *node = textnode_from_head(link);
        return node->totalWeight;
}

int child_weight(struct rb3_head *link, int dir)
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


static void augment_node(struct rb3_head *head)
{
        struct Textnode *node = textnode_from_head(head);
        node->totalWeight = own_weight(head)
                + child_weight(head, RB3_LEFT)
                + child_weight(head, RB3_RIGHT);
}

static void link_textnode(struct rb3_head *child, struct rb3_head *parent, int linkDir)
{
        rb3_link_and_rebalance_and_maybe_augment(child, parent, linkDir, &augment_node);
}

static void unlink_textnode(struct rb3_head *head)
{
        rb3_unlink_and_rebalance_and_maybe_augment(head, &augment_node);
}


void findpos(struct Textiter *iter, int pos)
{
        while (iter->current) {
                if (iter->pos >= pos)
                        go_to_left_child(iter);
                else if (iter->pos + own_weight(iter->current) <= pos)
                        go_to_right_child(iter);
                else
                        /* in range */
                        return;
        }
        if (iter->pos == pos)
                return;
        /* out of range. What to do? */
        ENSURE(0);
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

void insert_text_into_textrope(struct Textrope *rope, int pos, const char *text, int length)
{
        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);
        findpos(iter, pos);

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
                unlink_textnode(old);
                int d = pos - rangeStart;
                insert_text_into_textrope(rope, pos, oldNode->text + d, oldNode->ownWeight - d);
                insert_text_into_textrope(rope, pos, text, length);
                insert_text_into_textrope(rope, pos, oldNode->text, d);
        }
        else {
                /* Otherwise (if the position lies on one of the ends
                of the range) we can insert the new text here. */
                ENSURE(iter->current == NULL);
                struct Textnode *newNode = alloc_textnode(text, length);
                link_textnode(&newNode->link, iter->parent, iter->linkDir);
        }
}

int textrope_length(struct Textrope *rope)
{
        struct rb3_head *root = rb3_get_root(&rope->tree);
        if (root == NULL)
                return 0;
        return total_weight(root);
}


void erase_text_from_textrope(struct Textrope *textrope, int offset, int length)
{
        // TODO
}

int copy_text_from_textrope(struct Textrope *rope, int offset, char *dstBuffer, int length)
{
        // TODO
        return 42;
        struct Textiter textiter;
        struct Textiter *iter = &textiter;
        init_textiter(iter, rope);
}