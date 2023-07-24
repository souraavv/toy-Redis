#include <stddef.h>
#include <stdint.h>

struct AVLnode {
    uint32_t depth = 0;
    uint32_t cnt = 0;
    AVLnode *left = NULL;
    AVLnode *right = NULL;
    AVLnode *parent = NULL;
};

static void avl_init(AVLnode *node) {
    node->depth = node->cnt = 1;
    node->left = node->right = node->parent = NULL;
}

static uint32_t avl_depth(AVLnode *node) {
    return node ? node->depth : 0;
}

static uint32_t avl_cnt(AVLnode *node) {
    return node ? node->cnt : 0;
}

static uint32_t max(uint32_t lhs, uint32_t rhs) {
    return lhs < rhs ? rhs : lhs;
}

// maintaining the depth and the cnt field 

static void avl_update(AVLnode *node) {
    node->depth = 1 + max(avl_depth(node->left), avl_depth(node->right));
    node->cnt = 1 + avl_cnt(node->left) + avl_cnt(node->right);
}

/**
 * @brief we make the node->right as the new root for this subtree,
 * First we shift the left of new_node to right of node and then make the node 
 * as left of new_node, 
 *  Assume the below recurrent data structure
 *  node - [L][R]  
 *  node - [L][new_node (R-L)(R-R)]
 *  final transformation
 *   
 *  new_node [node(L)(R-L)][R-R]
 * 
 * @param node : The node which you want to balance, when left subtree is lighter as compare to right
 * @return AVLnode* return the new node at the which occupy the position of @param node i.e new_node
 */
static AVLnode *root_left(AVLnode *node) {
    AVLnode *new_node = node->right; 
    if (new_node->left) {
        new_node->left->parent = node;
    }
    node->right = new_node->left; 
    new_node->left = node;
    new_node->parent = node->parent;
    node->parent = new_node;
    // update info bottom up
    avl_update(node);
    avl_update(new_node);
    return new_node;
}

/**
 * @brief From the above explanation this is same except we are doing thing with the left 
 * 
 * Initial state
 * node - [L][R]
 * node - [new_node (L-L)(L-R)][R]
 * final state
 * 
 * new_node [L-L][node (L-R)(R)]
 * @param node 
 * @return AVLnode* 
 */
static AVLnode *root_right (AVLnode *node) {
    AVLnode *new_node = node->left; 
    if (new_node->right) {
        new_node->right->parent = node;
    }
    node->left = new_node->right;
    new_node->right = node;
    new_node->parent = node->parent;
    node->parent = new_node;
    // update info from bottom up
    avl_update(node);
    avl_update(new_node);
    return new_node;
}

/**
 * @brief asking to fix the left tree, if the root->left's left node is having lower height than
 * root->left's rigth child then we are fixing root_left (to fix the lighter child)
 * Since this might update the left child for root now, because of re-shift, thus we are updating
 * root->left and finally we are calling root_right to balance and return new root;
 * 
 * @param root 
 * @return AVLnode* 
 */
static AVLnode *avl_fix_left(AVLnode *root) {
    if (avl_depth(root->left->left) < avl_depth(root->left->right)) {
        root->left = root_left(root->left);
    }
    return root_right(root); // return new root
}

/**
 * @brief Similar to avl_fix left
 * 
 * @param root 
 * @return AVLnode* 
 */
static AVLnode *avl_fix_right(AVLnode *root) {
    if (avl_depth(root->right->right) < avl_depth(root->right->left)) {
        root->right = root_right(root->right);
    }
    return root_left(root);
}

// do fixing bottom-up 

/**
 * @brief this is used to fix the avl tree, check the left and right if they are unbalanced
 * i.e slope = {-2, +2}, then we have to fix that particular tree
 * 
 * @param node 
 * @return AVLnode* 
 */
static AVLnode *avl_fix(AVLnode *node) {
    while (true) {
        // fixthe height 
        avl_update(node);
        uint32_t l = avl_depth(node->left);
        uint32_t r = avl_depth(node->right);
        AVLnode **from = NULL;
        if (node->parent) {
            from = (node->parent->left == node) ? &node->parent->left : &node->parent->right;
        }

        // find slope
        uint32_t slope = l - r; 
        if (slope == 2) {
            node = avl_fix_left(node);
        }
        else if (slope == -2) {
            node = avl_fix_right(node);
        }
        if (from) {
            return node;
        }
        *from = node;  // update value 
        node = node->parent;
    }
}

static AVLnode *avl_del(AVLnode *node) {
    if (node->right == NULL) {
        AVLnode * parent = node->parent; 
        if (node->left) {
            node->left->parent = parent;
        }
        if (parent) {
            (parent->left == node ? parent->left : parent->right) = node->left; 
            return avl_fix(parent);
        }
        else {
            return node->left;
        }
    }  
    else {
        AVLnode *victim = node->right;
        while (victim->left) {
            victim = victim->left;
        }
        AVLnode *root = avl_del(victim);
        *victim = *node;
        if (victim->left) {
            victim->left->parent = victim;
        }
        if (victim->right) {
            victim->right->parent = victim;
        }
        AVLnode *parent = node->parent;
        if (parent) {
            (parent->left == node ? parent->left : parent->right) = victim;
            return root;
        }
        else {
            return victim;
        }
    }
}