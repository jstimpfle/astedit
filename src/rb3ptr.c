#include "rb3ptr.h"


#define _RB3_DIR_BIT (1<<0)
#define _RB3_COLOR_BIT (1<<1)
#define _RB3_BLACK (0)
#define _RB3_RED (_RB3_COLOR_BIT)
#define _RB3_CHILD_PTR(head, color) ((rb3_ptr)(head) | color)
#define _RB3_PARENT_PTR(head, dir) ((rb3_ptr)(head) | dir)


static inline struct rb3_head *rb3_get_black_child(struct rb3_head *head, int dir)
{
        return (struct rb3_head *) head->child[dir];
}

static inline int rb3_get_color_bit(struct rb3_head *head, int dir)
{
        return head->child[dir] & _RB3_COLOR_BIT;
}

static inline int rb3_is_red(struct rb3_head *head, int dir)
{
        return rb3_get_color_bit(head, dir) != 0;
}

static inline void rb3_set_red(struct rb3_head *head, int dir)
{
        head->child[dir] |= _RB3_COLOR_BIT;
}

static inline void rb3_set_black(struct rb3_head *head, int dir)
{
        head->child[dir] &= ~_RB3_COLOR_BIT;
}

static inline void rb3_connect(struct rb3_head *head, int dir, struct rb3_head *child, int color)
{
        head->child[dir] = _RB3_CHILD_PTR(child, color);
        child->parent = _RB3_PARENT_PTR(head, dir);
}

static inline void rb3_connect_null(struct rb3_head *head, int dir, struct rb3_head *child, int color)
{
        head->child[dir] = _RB3_CHILD_PTR(child, color);
        if (child)
                child->parent = _RB3_PARENT_PTR(head, dir);
}

_RB3_COLD void rb3_reset_tree(struct rb3_tree *tree)
{
        tree->base.child[RB3_LEFT] = 0;
        /* ! see doc of rb3_is_base(). */
        tree->base.child[RB3_RIGHT] = 3;
        tree->base.parent = 0;
}

struct rb3_tree *rb3_get_containing_tree(struct rb3_head *head)
{
        while (rb3_get_parent(head))
                head = rb3_get_parent(head);
        return (struct rb3_tree *) ((char *) head - (_RB3_offsetof(struct rb3_head, child[0])));
}

static _RB3_NEVERINLINE struct rb3_head *rb3_get_minmax_in_subtree(struct rb3_head *head, int dir)
{
        if (!head)
                return _RB3_NULL;
        while (rb3_has_child(head, dir))
                head = rb3_get_child(head, dir);
        return head;
}

struct rb3_head *rb3_get_minmax(struct rb3_tree *tree, int dir)
{
        return rb3_get_minmax_in_subtree(rb3_get_root(tree), dir);
}

struct rb3_head *rb3_get_prevnext_descendant(struct rb3_head *head, int dir)
{
        return rb3_get_minmax_in_subtree(rb3_get_child(head, dir), !dir);
}

struct rb3_head *rb3_get_prevnext_ancestor(struct rb3_head *head, int dir)
{
        /*
         * Note: the direction is "reversed" for our purposes here, since
         * the bit indicates the direction from the parent to `head`
         */
        while (head && rb3_get_parent_dir(head) == dir) {
                head = rb3_get_parent(head);
        }
        if (head) {
                head = rb3_get_parent(head);
                if (!head || rb3_is_base(head))
                        return _RB3_NULL;
                return head;
        }
        return _RB3_NULL;
}

struct rb3_head *rb3_get_prevnext(struct rb3_head *head, int dir)
{
        if (rb3_has_child(head, dir))
                return rb3_get_prevnext_descendant(head, dir);
        else
                return rb3_get_prevnext_ancestor(head, dir);
}

void rb3_update_augment(struct rb3_head *head, rb3_augment_func *augment)
{
        while (!rb3_is_base(head)) {
                augment(head);
                head = rb3_get_parent(head);
        }
}

/* insert implementation */

static void rb3_insert_rebalance(struct rb3_head *head, rb3_augment_func *augment)
{
        struct rb3_head *pnt;
        struct rb3_head *gpnt;
        struct rb3_head *ggpnt;
        int left;
        int right;
        int gdir;
        int ggdir;

        if (!rb3_get_parent(rb3_get_parent(head))) {
                rb3_set_black(rb3_get_parent(head), RB3_LEFT);
                if (augment)
                        augment(head);
                return;
        }

        if (!rb3_is_red(rb3_get_parent(rb3_get_parent(head)), rb3_get_parent_dir(rb3_get_parent(head)))) {
                /* parent is black */
                if (augment)
                        rb3_update_augment(head, augment);
                return;
        }

        /*
         * Since parent is red parent can't be the root.
         * So we have at least a grandparent node, and grand-grandparent
         * is either a real node or the base head.
         */
        pnt = rb3_get_parent(head);
        gpnt = rb3_get_parent(pnt);
        ggpnt = rb3_get_parent(gpnt);
        left = rb3_get_parent_dir(head);
        right = !rb3_get_parent_dir(head);
        gdir = rb3_get_parent_dir(pnt);
        ggdir = rb3_get_parent_dir(gpnt);

        if (rb3_is_red(gpnt, !gdir)) {
                /* uncle and parent are both red */
                rb3_set_red(ggpnt, ggdir);
                rb3_set_black(gpnt, RB3_LEFT);
                rb3_set_black(gpnt, RB3_RIGHT);
                if (augment)
                        rb3_update_augment(head, augment);
                rb3_insert_rebalance(gpnt, augment);
        } else if (gdir == right) {
                rb3_connect_null(pnt, left, rb3_get_black_child(head, right), _RB3_BLACK);
                rb3_connect_null(gpnt, right, rb3_get_black_child(head, left), _RB3_BLACK);
                rb3_connect(head, left, gpnt, _RB3_RED);
                rb3_connect(head, right, pnt, _RB3_RED);
                rb3_connect(ggpnt, ggdir, head, _RB3_BLACK);
                if (augment) {
                        augment(pnt);
                        augment(gpnt);
                        rb3_update_augment(head, augment);
                }
        } else {
                rb3_connect_null(gpnt, left, rb3_get_black_child(pnt, right), _RB3_BLACK);
                rb3_connect(pnt, right, gpnt, _RB3_RED);
                rb3_connect(ggpnt, ggdir, pnt, _RB3_BLACK);
                if (augment) {
                        augment(gpnt);
                        rb3_update_augment(head, augment);
                }
        }
}

void rb3_link_and_rebalance_and_maybe_augment(struct rb3_head *head, struct rb3_head *parent, int dir, rb3_augment_func *augment)
{
        _RB3_ASSERT(dir == RB3_LEFT || dir == RB3_RIGHT);
        _RB3_ASSERT(!rb3_has_child(parent, dir));

        parent->child[dir] = _RB3_CHILD_PTR(head, _RB3_RED);
        head->parent = _RB3_PARENT_PTR(parent, dir);
        head->child[RB3_LEFT] = _RB3_CHILD_PTR(_RB3_NULL, _RB3_BLACK);
        head->child[RB3_RIGHT] = _RB3_CHILD_PTR(_RB3_NULL, _RB3_BLACK);
        rb3_insert_rebalance(head, augment);
}

/* delete implementation */

static void rb3_delete_rebalance(struct rb3_head *pnt, int pdir, rb3_augment_func *augment)
{
        struct rb3_head *gpnt;
        struct rb3_head *sibling;
        struct rb3_head *sleft;
        struct rb3_head *sleftleft;
        struct rb3_head *sleftright;
        int left;
        int right;
        int gdir;

        if (!rb3_get_parent(pnt))
                return;

        left = pdir;  // define "left" as the direction from parent to deleted node
        right = !pdir;
        gpnt = rb3_get_parent(pnt);
        gdir = rb3_get_parent_dir(pnt);
        sibling = rb3_get_child(pnt, right);
        sleft = rb3_get_child(sibling, left);

        if (rb3_is_red(pnt, right)) {
                /* sibling is red */
                rb3_connect(pnt, right, sleft, _RB3_BLACK);
                rb3_connect(sibling, left, pnt, _RB3_RED);
                rb3_connect(gpnt, gdir, sibling, _RB3_BLACK);
                if (augment)
                        augment(sleft);
                rb3_delete_rebalance(pnt, pdir, augment);
        } else if (rb3_is_red(sibling, right)) {
                /* outer child of sibling is red */
                rb3_connect_null(pnt, right, sleft, rb3_get_color_bit(sibling, left));
                rb3_connect(sibling, left, pnt, _RB3_BLACK);
                rb3_connect(gpnt, gdir, sibling, rb3_get_color_bit(gpnt, gdir));
                if (augment) {
                        rb3_update_augment(pnt, augment);
                }
                rb3_set_black(sibling, right);
        } else if (rb3_is_red(sibling, left)) {
                /* inner child of sibling is red */
                sleftleft = rb3_get_child(sleft, left);
                sleftright = rb3_get_child(sleft, right);
                rb3_connect_null(pnt, right, sleftleft, _RB3_BLACK);
                rb3_connect_null(sibling, left, sleftright, _RB3_BLACK);
                rb3_connect(sleft, left, pnt, _RB3_BLACK);
                rb3_connect(sleft, right, sibling, _RB3_BLACK);
                rb3_connect(gpnt, gdir, sleft, rb3_get_color_bit(gpnt, gdir));
                if (augment) {
                        augment(sibling);
                        rb3_update_augment(pnt, augment);
                }
        } else if (rb3_is_red(gpnt, gdir)) {
                /* parent is red */
                rb3_set_red(pnt, right);
                rb3_set_black(gpnt, gdir);
                if (augment)
                        rb3_update_augment(pnt, augment);
        } else {
                /* all relevant nodes are black */
                rb3_set_red(pnt, right);
                if (augment)
                        augment(pnt);
                rb3_delete_rebalance(gpnt, gdir, augment);
        }
}


void rb3_replace_and_maybe_augment(struct rb3_head *head, struct rb3_head *newhead, rb3_augment_func *augment)
{
        struct rb3_head *left;
        struct rb3_head *right;
        struct rb3_head *parent;
        int pdir;
        int pcol;

        *newhead = *head;

        left = rb3_get_child(head, RB3_LEFT);
        right = rb3_get_child(head, RB3_RIGHT);
        parent = rb3_get_parent(head);
        pdir = rb3_get_parent_dir(head);
        pcol = rb3_get_color_bit(parent, pdir);

        if (left)
                left->parent = _RB3_PARENT_PTR(newhead, RB3_LEFT);
        if (right)
                right->parent = _RB3_PARENT_PTR(newhead, RB3_RIGHT);
        parent->child[pdir] = _RB3_CHILD_PTR(newhead, pcol);

        if (augment)
                rb3_update_augment(newhead, augment);
}

static _RB3_NEVERINLINE void rb3_delete_noninternal(struct rb3_head *head, rb3_augment_func *augment)
{
        struct rb3_head *pnt;
        struct rb3_head *cld;
        int pdir;
        int dir;

        dir = rb3_get_child(head, RB3_RIGHT) ? RB3_RIGHT : RB3_LEFT;

        pnt = rb3_get_parent(head);
        cld = rb3_get_child(head, dir);
        pdir = rb3_get_parent_dir(head);

        int mustRebalance = !rb3_is_red(pnt, pdir) && !rb3_is_red(head, dir);

        /* since we added the possibility for augmentation,
        we need to remove `head` *before* the rebalancing that we do below.
        (Otherwise the augmentation function would still see the to-be-deleted child). */
        rb3_connect_null(pnt, pdir, cld, _RB3_BLACK);

        if (mustRebalance)
                /* To be deleted node is black (and child cannot be repainted)
                 * => height decreased */
                rb3_delete_rebalance(pnt, pdir, augment);
        else if (augment)
                /* the augment wasn't done since we didn't rebalance. So we need to do it separately.
                TODO: Could we restrict the augmentation done during rebalancing to just the
                nodes that aren't not be augmented by a regular rb3_augment_ancestors(pnt, augment)? */
                rb3_update_augment(pnt, augment);
}

static void rb3_delete_internal(struct rb3_head *head, rb3_augment_func *augment)
{
        struct rb3_head *subst;

        subst = rb3_get_next_descendant(head);
        rb3_delete_noninternal(subst, augment);
        rb3_replace_and_maybe_augment(head, subst, augment);
}

void rb3_unlink_and_rebalance_and_maybe_augment(struct rb3_head *head, rb3_augment_func *augment)
{
        if (rb3_has_child(head, RB3_LEFT) && rb3_has_child(head, RB3_RIGHT))
                rb3_delete_internal(head, augment);
        else
                rb3_delete_noninternal(head, augment);
}

/* node-find implementations using code from inline header functions */

struct rb3_head *rb3_find_parent_in_subtree(struct rb3_head *parent, int dir, rb3_cmp cmp, void *data, struct rb3_head **parent_out, int *dir_out)
{
        return rb3_INLINE_find(parent, dir, cmp, data, parent_out, dir_out);
}

/* find, insert, delete with rb3_datacmp */

struct rb3_head *rb3_insert(struct rb3_tree *tree, struct rb3_head *head, rb3_cmp cmp, void *data)
{
        struct rb3_head *found;
        struct rb3_head *parent;
        int dir;

        parent = rb3_get_base(tree);
        dir = RB3_LEFT;
        found = rb3_find_parent_in_subtree(parent, dir, cmp, data, &parent, &dir);
        if (found)
                return found;
        rb3_link_and_rebalance(head, parent, dir);
        return _RB3_NULL;
}

struct rb3_head *rb3_delete(struct rb3_tree *tree, rb3_cmp cmp, void *data)
{
        struct rb3_head *found;
        
        found = rb3_find(tree, cmp, data);
        if (found) {
                rb3_unlink_and_rebalance(found);
                return found;
        }
        return _RB3_NULL;
}




struct rb3_head *rb3_find_parent(struct rb3_tree *tree, rb3_cmp cmp, void *data, struct rb3_head **parent_out, int *dir_out)
{
        return rb3_find_parent_in_subtree(rb3_get_base(tree), RB3_LEFT, cmp, data, parent_out, dir_out);
}

struct rb3_head *rb3_find(struct rb3_tree *tree, rb3_cmp cmp, void *data)
{
        return rb3_find_parent_in_subtree(rb3_get_base(tree), RB3_LEFT, cmp, data, _RB3_NULL, _RB3_NULL);
}





void rb3_link_and_rebalance(struct rb3_head *head, struct rb3_head *parent, int dir)
{
        rb3_link_and_rebalance_and_maybe_augment(head, parent, dir, _RB3_NULL);
}

void rb3_unlink_and_rebalance(struct rb3_head *head)
{
        rb3_unlink_and_rebalance_and_maybe_augment(head, _RB3_NULL);
}

void rb3_replace(struct rb3_head *head, struct rb3_head *newhead)
{
        rb3_replace_and_maybe_augment(head, newhead, _RB3_NULL);
}



void rb3_link_and_rebalance_and_augment(struct rb3_head *head, struct rb3_head *parent, int dir, rb3_augment_func *augment)
{
        rb3_link_and_rebalance_and_maybe_augment(head, parent, dir, augment);
}

void rb3_unlink_and_rebalance_and_augment(struct rb3_head *head, rb3_augment_func *augment)
{
        rb3_unlink_and_rebalance_and_maybe_augment(head, augment);
}

void rb3_replace_and_augment(struct rb3_head *head, struct rb3_head *newhead, rb3_augment_func *augment)
{
        rb3_replace_and_maybe_augment(head, newhead, augment);
}






/* DEBUG STUFF */

#include <stdio.h>
static void visit_inorder_helper(struct rb3_head *head, int isred)
{
        if (!head)
                return;
        printf(" (");
        visit_inorder_helper(rb3_get_child(head, RB3_LEFT), rb3_is_red(head, RB3_LEFT));
        printf("%s", isred ? "R" : "B");
        visit_inorder_helper(rb3_get_child(head, RB3_RIGHT), rb3_is_red(head, RB3_RIGHT));
        printf(")");
}

static void visit_inorder(struct rb3_tree *tree)
{
        visit_inorder_helper(rb3_get_root(tree), 0);
        printf("\n");
}

static int rb3_is_valid_tree_helper(struct rb3_head *head, int isred, int dir, int *depth)
{
        int i;
        int depths[2] = { 1,1 };

        *depth = 1;

        if (!head) {
                if (isred) {
                        printf("red leaf child!\n");
                        return 0;
                }
                return 1;
        }

        if (rb3_get_parent_dir(head) != dir) {
                printf("Directions messed up!\n");
                return 0;
        }

        for (i = 0; i < 2; i++) {
                if (isred && rb3_get_color_bit(head, i)) {
                        printf("two red in a row!\n");
                        return 0;
                }
                if (!rb3_is_valid_tree_helper(rb3_get_child(head, i),
                        rb3_is_red(head, i), i, &depths[i]))
                        return 0;
        }
        if (depths[0] != depths[1]) {
                printf("Unbalanced tree! got %d and %d\n", depths[0], depths[1]);
                return 0;
        }
        *depth = depths[0] + !isred;

        return 1;
}

int rb3_check_tree(struct rb3_tree *tree)
{
        int depth;
        int valid;

        if (rb3_is_red(&tree->base, RB3_LEFT)) {
                printf("Error! root is red.\n");
                return 0;
        }

        valid = rb3_is_valid_tree_helper(rb3_get_root(tree), 0, 0, &depth);
        if (!valid)
                visit_inorder(tree);
        return valid;
}