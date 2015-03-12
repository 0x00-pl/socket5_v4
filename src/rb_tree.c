#include "rb_tree.h"




static void left_rotate(rb_node **proot, rb_node *n){
    rb_node * new_n = n->right;
    
    n->right = new_n->left; 
    if(new_n->left != NULL) {n->right->parent = n;}
    
    new_n->parent = n->parent;
    if(n->parent == NULL){
        *proot = new_n;
    }
    else{
        *(n==n->parent->left? &n->parent->left: &n->parent->right) = new_n;
    }
    
    new_n->left = n;
    n->parent = new_n;
}

static void right_rotate(rb_node **proot, rb_node *n){
    rb_node * new_n = n->left;
    
    n->left = new_n->right; 
    if(new_n->right != NULL) {n->left->parent = n;}
    
    new_n->parent = n->parent;
    if(n->parent == NULL){
        *proot = new_n;
    }
    else{
        *(n==n->parent->left? &n->parent->left: &n->parent->right) = new_n;
    }
    
    new_n->right = n;
    n->parent = new_n;
}


void rbt_after_insert(rb_node **proot, rb_node *n){
    rb_node *u;
    if(*proot!=NULL) {(*proot)->color = RBT_COLOR_BLACK;}
    n->color = RBT_COLOR_RED;
    n->left = n->right = NULL;
    if(n->parent == NULL) {*proot = n; return;}
    // A (uppercase) is red; a (lowercase) is black.
    while(n->parent!=NULL && n->parent->color==RBT_COLOR_RED){
        if(n->parent->parent->left == n->parent){
            u = n->parent->parent->right;
            if(u!=NULL && u->color==RBT_COLOR_RED){
                /*
                * Case 1
                *
                *       g            G
                *      / \          / \
                *     P   U  -->   p   u
                *    /            /
                *   N            N
                *
                */
                u->color = RBT_COLOR_BLACK;
                n->parent->color = RBT_COLOR_BLACK;
                n->parent->parent->color = RBT_COLOR_RED;
                n = n->parent->parent;
                if(n->parent == NULL) {break;}
            }
            else{
                if(n->parent->right == n){
                    /*
                    * Case 2
                    *
                    *      g             g
                    *     / \           / \
                    *    P   u  -->    N'  u
                    *     \           /
                    *      N         P'
                    */
                    n = n->parent;
                    left_rotate(proot, n);
                }
                /*
                * Case 3
                *
                *        g           p
                *       / \         / \
                *      P   u  -->  N   G
                *     /                 \
                *    N                   u
                */
                n->parent->color = RBT_COLOR_BLACK;
                n->parent->parent->color = RBT_COLOR_RED;
                right_rotate(proot, n->parent->parent);
            }
        }
        else{
            u = n->parent->parent->left;
            if(u!=NULL && u->color==RBT_COLOR_RED){
                // Case 1
                u->color = RBT_COLOR_BLACK;
                n->parent->color = RBT_COLOR_BLACK;
                n->parent->parent->color = RBT_COLOR_RED;
                n = n->parent->parent;
            }
            else{
                if(n->parent->left == n){
                    // Case 2
                    n = n->parent;
                    right_rotate(proot, n);
                }
                // Case 3
                n->parent->color = RBT_COLOR_BLACK;
                n->parent->parent->color = RBT_COLOR_RED;
                left_rotate(proot, n->parent->parent);
            }
        }
    }
    
    (*proot)->color = RBT_COLOR_BLACK;
}


rb_node *rbt_min(rb_node *n){
    if(n == NULL) {return NULL;}
    while(n->left != NULL){
        n = n->left;
    }
    return n;
}

rb_node *rbt_max(rb_node *n){
    if(n == NULL) {return NULL;}
    while(n->right != NULL){
        n = n->right;
    }
    return n;
}

rb_node *rbt_prev(rb_node *n){
    if(n->left != NULL){
        return rbt_max(n->left);
    }else{
        while(n->parent != NULL){
            if(n->parent->right==n){
                return n->parent;
            }
            n = n->parent;
        }
        return NULL;
    }
}

rb_node *rbt_next(rb_node *n){
    if(n->right != NULL){
        return rbt_min(n->right);
    }else{
        while(n->parent != NULL){
            if(n->parent->left==n){
                return n->parent;
            }
            n = n->parent;
        }
        return NULL;
    }
}


static int rbt_null_or_black(rb_node *n){
    return n==NULL || n->color==RBT_COLOR_BLACK;
}

rb_node *rbt_replace(rb_node **proot, rb_node *old_node, rb_node *new_node){
        new_node->parent = old_node->parent;
        
        new_node->left = old_node->left;
        new_node->right = old_node->right;
        
        if(old_node->parent == NULL){
            *proot = new_node;
        }
        else{
            *(old_node->parent->left==old_node? &old_node->parent->left: &old_node->parent->right) = new_node;
        }
        
        if(old_node->left != NULL){
            old_node->left->parent = new_node;
        }
        if(old_node->right != NULL){
            old_node->right->parent = new_node;
        }
        
        new_node->color = old_node->color;
    return old_node;
}

rb_node *rbt_pop(rb_node **proot, rb_node *n){
    rb_node *ret = n;
    rb_node *sibling;
    rb_node *x;
    if(n->left != NULL && n->right != NULL){
        n = rbt_next(n);
    }
    x = n;
    
    // fix color first
    if(n->color == RBT_COLOR_BLACK){
        n->color = RBT_COLOR_RED;
        // remove black node
        while(n!=NULL && n->color==RBT_COLOR_BLACK){
            if(n->parent->left == n){
                sibling = n->parent->right;
                if(sibling->color == RBT_COLOR_RED){
                    /*
                     * Case 1
                     * 
                     *   p           si 
                     *  / \   -->   / \
                     * n  SI       P
                     *   / \      / \
                     *           n
                     * 
                     */
                    sibling->color = RBT_COLOR_BLACK;
                    n->parent->color = RBT_COLOR_RED;
                    left_rotate(proot, n);
                    sibling = n->parent->right;
                }
                if(rbt_null_or_black(sibling->left) && rbt_null_or_black(sibling->right)){
                    /*
                     * Case 2
                     * 
                     *   p           p' 
                     *  / \   -->   / \
                     * n  si       n' SI
                     * 
                     */
                    sibling->color = RBT_COLOR_RED;
                    n = n->parent;
                }
                else{
                    if(rbt_null_or_black(sibling->right)){
                        /*
                        * Case 3
                        * 
                        *   p           p 
                        *  / \   -->   / \
                        * n  si       n  R
                        *   / \           \
                        *  R   b          si
                        * 
                        */
                        sibling->left->color = RBT_COLOR_BLACK;
                        sibling->color = RBT_COLOR_RED;
                        right_rotate(proot, sibling);
                        sibling = n->parent->right;
                    }
                    /*
                     * Case 4
                     * 
                     *   p           p 
                     *  / \   -->   / \
                     * n  si       n  R
                     *   / \           \
                     *  b   R          si
                     * 
                     */
                    sibling->color = n->parent->color;
                    n->parent->color = RBT_COLOR_BLACK;
                    sibling->right->color = RBT_COLOR_BLACK;
                    left_rotate(proot, n->parent);
                    n = *proot;
                    break;
                }
            }
            else{
                sibling = n->parent->left;
                if(sibling->color == RBT_COLOR_RED){
                    // Case 1
                    sibling->color = RBT_COLOR_BLACK;
                    n->parent->color = RBT_COLOR_RED;
                    right_rotate(proot, n);
                    sibling = n->parent->left;
                }
                if(rbt_null_or_black(sibling->left) && rbt_null_or_black(sibling->right)){
                    // Case 2
                    sibling->color = RBT_COLOR_RED;
                    n = n->parent;
                }
                else{
                    if(rbt_null_or_black(sibling->left)){
                        // Case 3
                        sibling->right->color = RBT_COLOR_BLACK;
                        sibling->color = RBT_COLOR_RED;
                        left_rotate(proot, sibling);
                        sibling = n->parent->left;
                    }
                    // Case 4
                    sibling->color = n->parent->color;
                    n->parent->color = RBT_COLOR_BLACK;
                    sibling->left->color = RBT_COLOR_BLACK;
                    right_rotate(proot, n->parent);
                    n = *proot;
                    break;
                }
            }
        }
        n->color = RBT_COLOR_BLACK;
    }
    
    // pop x
    if(x->left != NULL){
        /*
         *  |       |
         *  x  -->  l
         * /
         * l
         * 
         */
        x->left->parent = x->parent;
        if(x->parent == NULL){
            *proot = x->left;
        }else{
            *(x->parent->left==x? &x->parent->left: &x->parent->right) = x->left;
        }
//         *(x->parent==NULL? proot: &x->parent->left) = x->left;
    }
    else if(x->right != NULL){
        x->right->parent = x->parent;
        if(x->parent == NULL){
            *proot = x->right;
        }else{
            *(x->parent->left==x? &x->parent->left: &x->parent->right) = x->right;
        }
//         *(x->parent==NULL? proot: &x->parent->right) = x->right;
    }else{
        // n has no child.
        if(x->parent == NULL){
            *proot = NULL;
        }else{
            *(x->parent->left==x? &x->parent->left: &x->parent->right) = NULL;
        }
    }
    
    // replace ret to x
    if(x != ret){
        rbt_replace(proot, ret, x);
//         x->parent = ret->parent;
//         
//         x->left = ret->left;
//         x->right = ret->right;
//         
//         if(ret->parent == NULL){
//             *proot = x;
//         }
//         else{
//             *(ret->parent->left==ret? &ret->parent->left: &ret->parent->right) = x;
//         }
//         
//         if(ret->left != NULL){
//             ret->left->parent = x;
//         }
//         if(ret->right != NULL){
//             ret->right->parent = x;
//         }
//         
//         x->color = ret->color;
    }
    
    ret->left = ret->right = ret->parent = NULL;
    ret->color = RBT_COLOR_RED;
    return ret;
}


