#ifndef _RB_TREE_H_
#define _RB_TREE_H_

#include <stddef.h>

typedef struct rb_node_decl{
    size_t color;
#define RBT_COLOR_BLACK 0
#define RBT_COLOR_RED   1
    struct rb_node_decl *parent;
    struct rb_node_decl *left;
    struct rb_node_decl *right;
} rb_node;


void rbt_after_insert(rb_node **proot, rb_node *n);

rb_node *rbt_min(rb_node *n);
rb_node *rbt_max(rb_node *n);
rb_node *rbt_prev(rb_node *n);
rb_node *rbt_next(rb_node *n);

rb_node *rbt_replace(rb_node **proot, rb_node *old_node, rb_node *new_node);
rb_node *rbt_pop(rb_node **proot, rb_node *n);

#ifdef PL_TEST

int rb_tree_test();

#endif

#endif