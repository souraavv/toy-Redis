#include <assert.h> 
#include <stdlib.h> 

#include "hashtable.h"


const size_t MAX_LOAD_FACTOR = 8;
const size_t RESIZE_WORK = 128;


/// @brief initialize the hashtable with default value, like size
/// @param hash_table : pointer to the HashTable for which we are doing init 
/// @param n : size of hashtable, for simplicity this is kept as a power of 2
static void hashtable_init(HashTable *hash_table, size_t n) {
    assert (n > 0 && ((n - 1) & n) == 0);
    hash_table->container = (Hnode **) calloc (sizeof(Hnode *), n);
    hash_table->mask = n - 1; // this is all set n = 2^k thus 2^k - 1
    hash_table->size = 0;
}

/// @brief Insert a new node the given hashtable
/// @param hash_table : Table in which you want to insert 
/// @param node : node which you want to insert
void hashtable_insert(HashTable *hash_table, Hnode *node) {
    size_t pos = node->hcode & hash_table->mask; // isn't this is same as node->hcode ? 
    assert(pos <= hash_table->mask);
    Hnode *next = hash_table->container[pos];
    // insert at the head;
    node->next = next;
    hash_table->container[pos] = node;
    hash_table->size++;  // total elements
}

/// @brief Lookup a given key in the hashtable
/// @param hash_table : Table in which you want to lookup
/// @param key : Key which you want to look
/// @param cmp : Comparator function
/// @return the node if that exists
static Hnode **hashtable_lookup(HashTable *hash_table, Hnode *key, bool(*cmp)(Hnode*, Hnode*)) {
    if (!hash_table->container) {
        return NULL;
    }
    size_t pos = key->hcode & hash_table->mask;
    assert (pos <= hash_table->mask);
    Hnode **from = &hash_table->container[pos]; // to hold the address of a pointer we need pointer to pointer 
    while (*from) {
        if (cmp(*from, key)) {
            return from; // ok, got that.
        }
        from = &(*from)->next; // since from is a pointer to pointer to Hnode, thus we are passing & of the next Hnode
    }
    return NULL; // doesn't exists
}

/// @brief Remove a given node from the hashtable
/// @param hash_table : A pointer to the hashtable
/// @param from : match existing node in the hashtable to this node
/// @return exact ref to the node in the hashtable which matches from for further operation
static Hnode * hashtable_pop(HashTable *hash_table, Hnode **from) {
    Hnode *node = *from;
    *from = (*from)->next; 
    hash_table->size--;
    return node;
}


/// @brief This is helper function for dynamic resizing, without putting load
/// @param hash_map 

static void helper_resizing(HashMap *hash_map) {
    // if nothing to move from hashtable 2
    if (hash_map->ht2.container == NULL) {
        return;
    }
    size_t work = 0;
    while (work < RESIZE_WORK && hash_map->ht2.size > 0) {
        Hnode **from = &hash_map->ht2.container[hash_map->resizing_pos]; // get the node from hashtable2
        if (!*from) {
            // If the list is empty then nothing to do, simply go to the next index
            hash_map->resizing_pos++; 
            continue;
        }
        // insert the value which are temporary passed to the hashtable 2 back to the hashtable 1
        // wondering why we are not increasing resizing pos here ? Note that we are poping out from from hash_map 
        hashtable_insert(&hash_map->ht1, hashtable_pop(&hash_map->ht2, from));
        work++;
    }
    // if nothing to pick from the hashtable then congrats, you have successfully moved
    // note here container is not NULL, but size is 0, thus we are marking container null at the end of this
    if (hash_map->ht2.size == 0) { 
        free(hash_map->ht2.container); // free the table
        hash_map->ht2 = HashTable{};
    }
}

/// @brief Lookup function, where in every query we are gradually moving and resizing to avoid move in single go
/// @param hash_map : Pointer to hash_map
/// @param key : Node which we are searching
/// @param cmp : A comparator function
/// @return node which are looking, actual pointer to the location, so that we can erase that

Hnode *hashmap_lookup(HashMap *hash_map, Hnode* key, bool(*cmp)(Hnode*, Hnode*)) {
    helper_resizing(hash_map);
    Hnode **from = hashtable_lookup(&hash_map->ht1, key, cmp);
    if (!from) {
        from = hashtable_lookup(&hash_map->ht2, key, cmp);
    }
    return from ? *from : NULL; // If rust was then we don't have to worry about the NULL pointer de-ref :)
}


/**
 * @brief This is a resizing function. When size of fall short then we are filling
 * @param hash_map 
 * @return * void 
 */
static void start_resizing(HashMap *hash_map) {
    assert (hash_map->ht2.container == NULL);
    // create a bigger hash_table and resize
    hash_map->ht2 = hash_map->ht1; 
    hashtable_init(&hash_map->ht1, (hash_map->ht1.mask + 1) * 2); // double the size
    hash_map->resizing_pos = 0; // reset so that next time, ht1 start picking from index 0 of hashtable 2.
}

/**
 * @brief insert the node into hashmap
 * 
 * @param hash_map : insert into this hashmap
 * @param node : node which is getting to the hashmap
 */
void hashmap_insert(HashMap *hash_map, Hnode* node) {
    if (hash_map->ht1.container == NULL) {
        hashtable_init(&hash_map->ht1, 4);
    }
    hashtable_insert(&hash_map->ht1, node);

    if (hash_map->ht2.container == NULL) {
        // check whether we need to resize at this moment ? 
        size_t load_factor = hash_map->ht1.size / (hash_map->ht1.mask + 1);
        if (load_factor >= MAX_LOAD_FACTOR) {
            start_resizing(hash_map);
        }
    }
    // dealying and slowing doing the operation 
    helper_resizing(hash_map);
}


/**
 * @brief pop the element from the hashmap
 * 
 * @param hash_map : Hashmap from which we need to pop 
 * @param key : key which we want to pop
 * @param cmp : comparator function 
 * @return Hnode* : the node which we want to pop
 */
Hnode * hashmap_pop(HashMap *hash_map, Hnode *key, bool(*cmp)(Hnode *, Hnode *)) {
    helper_resizing(hash_map); 
    // first try to pop it from the hastable 1
    Hnode **from = hashtable_lookup(&hash_map->ht1, key, cmp);

    if (from) {
        return hashtable_pop(&hash_map->ht1, from);
    }
    // and then try to pop it from hashtable 2
    from = hashtable_lookup(&hash_map->ht2, key, cmp);
    if (from) {
        return hashtable_pop(&hash_map->ht2, from);
    }
    return NULL;
}


/**
 * @brief Destroy the full hashmap, clean all the memory
 * @param hash_map : hash_map which we want to clean
 */
void hashmap_destroy(HashMap *hash_map) {
    assert(hash_map->ht1.size + hash_map->ht2.size == 0);
    free(hash_map->ht1.container); // free up container ht1
    free(hash_map->ht2.container); // free up container ht2
    *hash_map = HashMap{}; // update the content to a fresh container.
}