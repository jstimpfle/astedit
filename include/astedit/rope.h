#ifndef RB3PTR_H_INCLUDED
#error rb3ptr.h must be included first!
#endif

#ifdef ROPE_H_INCLUDED
#error rope.h included twice!
#endif
#define ROPE_H_INCLUDED




struct Node {
        struct rb3_head link;
        char name[4];  // for debugging
        int totalLength;
        int ownLength;
};

struct Rope {
        struct rb3_tree tree;
        struct Node firstNode;
        struct Node lastNode;
};

void init_rope(struct Rope *rope);
void init_node(struct Node *node, const char *name, int length);
struct Node *get_inorder_node(struct Node *node, enum Rb3Dir dir);
struct Node *get_first_intersecting_node(struct Rope *rope, int start, int length);
void insert_node_next_to_existing(struct Node *existingNode, struct Node *newNode, enum Rb3Dir dir);
void unlink_node(struct Node *node);