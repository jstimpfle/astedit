#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#define ASSERT assert

#include <rb3ptr.h>
#include <astedit/rope.h>


struct Node *node_from_head(struct rb3_head *head)
{
        return (struct Node *) ((char *)head - offsetof(struct Node, link));
}

struct Node *get_inorder_node(struct Node *node, enum Rb3Dir dir)
{
        struct rb3_head *head = rb3_get_prevnext(&node->link, dir);
        //struct rb3_head *head = rb3_get_prevnext_ancestor(&node->link, dir);
        if (head == NULL)
                return NULL;
        return node_from_head(head);
}

struct Node *get_child_node(struct Node *node, enum Rb3Dir dir)
{
        struct rb3_head *head = rb3_get_child(&node->link, dir);
        if (head == NULL)
                return NULL;
        return node_from_head(head);
}

int get_child_weight(struct Node *node, enum Rb3Dir dir)
{
        struct Node *child = get_child_node(node, dir);
        if (child == NULL)
                return 0;
        return child->totalLength;
}

void augment_node(struct rb3_head *head)
{
        ASSERT(!rb3_is_base(head));
        struct Node *node = node_from_head(head);
        int wleft = get_child_weight(node, RB3_LEFT);
        int wright = get_child_weight(node, RB3_RIGHT);
        node->totalLength = node->ownLength + wleft + wright;
}

#include <string.h>
void init_rope(struct Rope *rope)
{
        rb3_reset_tree(&rope->tree);
        init_node(&rope->firstNode, "FIRST", 4);
        init_node(&rope->lastNode, "LAST", 4);
        // cheat a little here...
        rope->firstNode.ownLength = 0;
        rope->firstNode.totalLength = 0;
        rope->lastNode.ownLength = 0;
        rope->lastNode.totalLength = 0;
        rb3_link_and_rebalance(&rope->firstNode.link, &rope->tree.base, RB3_LEFT);
        rb3_link_and_rebalance(&rope->lastNode.link, &rope->firstNode.link, RB3_RIGHT);
}

void init_node(struct Node *node, const char *name, int length)
{
        memset(&node->link, 0, sizeof node->link);
        node->ownLength = length;
        node->totalLength = length;
        memcpy(node->name, name, length < sizeof node->name ? length : sizeof node->name);
}

struct Node *get_first_intersecting_node(struct Rope *rope, int start, int length, int *outNodestart)
{
        struct Node *node = node_from_head(rb3_get_root(&rope->tree));
        //if (node == NULL)
        if (node == &rope->firstNode || node == &rope->lastNode) {
                ASSERT(node->totalLength == 0);
                return NULL;
        }
        int end = get_child_weight(node, RB3_LEFT) + node->ownLength;

        for (;;) {
                if (end <= start) {
                        struct Node *tmp = get_child_node(node, RB3_RIGHT);
                        //if (tmp == NULL)
                        if (tmp == NULL || tmp == &rope->lastNode)
                                break;
                        node = tmp;
                        end += get_child_weight(node, RB3_LEFT);
                        end += node->ownLength;
                }
                else {
                        struct Node *tmp = get_child_node(node, RB3_LEFT);
                        //if (tmp == NULL)
                        if (tmp == NULL || tmp == &rope->firstNode)
                                break;
                        end -= node->ownLength;
                        node = tmp;
                        end -= get_child_weight(node, RB3_RIGHT);
                }
        }

        if (end <= start) {
                struct Node *tmp = get_inorder_node(node, RB3_RIGHT);
                //if (tmp != NULL)
                if (tmp != NULL && tmp != &rope->lastNode) {
                        node = tmp;
                        end += node->ownLength;
                        ASSERT(end > start);
                }
        }

        if (end <= start)
                return NULL;
        if (end - node->ownLength >= start + length)
                return NULL;
        *outNodestart = end - node->ownLength;
        return node;
}

void insert_node_next_to_existing(struct Node *existingNode, struct Node *newNode, enum Rb3Dir dir)
{
        struct rb3_head *child = &newNode->link;
        struct rb3_head *head = &existingNode->link;
        struct rb3_head *parent;
        int insertDir;
        if (rb3_has_child(head, dir)) {
                parent = rb3_get_prevnext(head, dir);
                insertDir = !dir;
                ASSERT(!rb3_has_child(parent, insertDir));
        }
        else {
                parent = head;
                insertDir = dir;
        }
        rb3_link_and_rebalance_and_maybe_augment(child, parent, insertDir, &augment_node);
}


void unlink_node(struct Node *node)
{
        rb3_unlink_and_rebalance_and_maybe_augment(&node->link, &augment_node);
}

int rope_length(struct Rope *rope)
{
        int length = 0;
        for (struct Node *node = &rope->firstNode;
                node != NULL; node = get_child_node(node, RB3_RIGHT)) {
                length += get_child_weight(node, RB3_LEFT);
                length += node->ownLength;
        }
        return length;
}