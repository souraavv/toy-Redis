#include <stddef.h> 
#include <stdint.h> 


// node for hashtable 

struct Hnode {
    uint64_t val = 0;
    Hnode* next = NULL;
};

struct HashTable {
    Hnode **container = NULL;
    size_t mask = 0;
    size_t size = 0;
};

struct HashMap {
    HashTable ht1; 
    HashTable ht2; 
    size_t resizing_pos = 0;
};

Hnode query(HashMap *hmap, Hnode *key, bool(*cmp)(Hnode*, Hnode*));
void insert(HashMap *hmap, Hnode *node);
void pop(HashMap *hmap, Hnode *key, bool(*cmp)(Hnode*, Hnode*));
void destroy(HashMap *hmap);