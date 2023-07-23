#include <stddef.h> 
#include <stdint.h> 


// node for hashtable 

struct Hnode {
    uint64_t hcode = 0;
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

// basic api to the hashmap i.e lookup, insert, pop and destroy

Hnode *hashmap_lookup(HashMap *hash_map, Hnode* key, bool(*cmp)(Hnode*, Hnode*));
void hashmap_insert(HashMap *hash_map, Hnode* node);
Hnode * hashmap_pop(HashMap *hash_map, Hnode *key, bool(*cmp)(Hnode *, Hnode *));
void hashmap_destroy(HashMap *hash_map);