#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <set>
#include "avl.cpp"  // lazy


#define container_of(ptr, type, member) ({ \
    const typeof( ((type*)0)->member) *__mptr = (ptr);\
    (type *) ( (char*)__mptr - offsetof(type, member));})

struct Data {
    AVLnode node;
    uint32_t val = 0;
};

struct Container {
    AVLnode *root = NULL;
};

static void add (Container &c, uint32_t val) {
    Data *data = new Data();
    avl_init(&data->node);
    data->val = val; 

    if (!c.root) {
        c.root = &data->node;
        return;
    }
    AVLnode *cur = c.root;
    while (true) {
        AVLnode **from = (val < container_of(cur, Data, node)->val) ? &cur->left : &cur->right;
        // the place where node want to go is null
        if (!*from) {
            *from = &data->node; // insert the new node, same which is hold by data(this allow us to use container_of)
            data->node.parent = cur;  // since it is going either to cur->left or cur->right and thus cur is parent 
            c.root = avl_fix(&data->node); // start fixing from this node 
            break;
        }
        cur = *from; // move to the node if that is not the right place, since we go the right place that is NULL, and then we start fixing bottom up using avl_fix() 
    }
}

static bool del(Container &c, uint32_t val) {
    AVLnode *cur = c.root;
    while (cur) {
        uint32_t node_val = container_of(cur, Data, node)->val;
        if (val == node_val) { // got the node
            break;
        }
        cur = val < node_val ? cur->left : cur->right;
    }
    if (!cur) {
        return false;
    }
    c.root = avl_del(cur);
    delete container_of(cur, Data, node); // freeing up container is the job of the one who made this outer wrapper i.e user program it self. I think much cleaner way to do this is using template, but any way
    return true;
}

static void dispose(Container &c) {
    while (c.root) {
        AVLnode *node = c.root; 
        c.root = avl_del(c.root);
        delete container_of(node, Data, node);
    }
}


static void extract(AVLnode *node, std::multiset<uint32_t> &extracted) {
    if (!node) {
        return;
    }
    extract(node->left, extracted);
    extracted.insert(container_of(node, Data, node)->val);
    extract(node->right, extracted);
}

