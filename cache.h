/*
 * truck.h
 * Header of cache.
 *
 * Andy Wang: andiwang
 * Peter Xia: fx
 *
 * Spring 2014
 * 15-213 ProxyLab
 * Carnegie Mellon University
 *
 * The cache is implemented by list updating. With a singly linked list, an
 * object is moved to the end of the list when it was hit. The LRU object is
 * therefore always in the front of the list.
 */

#include "csapp.h"


struct cache_header;

/*
 * struct of single cache block
 */
struct cache_block {
    struct cache_block *next;
    size_t object_size;
    char *object_name;
    void *object;
};

/*
 * Init a cache object.
 */
struct cache_header *cache_init ();

/*
 * Find an object in the cache with the given uri. If the object exists in
 * cache, a pointer to it is returned. Otherwise NULL is returned.
 *
 * If the object exists in cache, it also automatically perform a cache hit
 * operation.
 */
struct cache_block *cache_find (struct cache_header *C, char *name);

/*
 * Add a data object to the cache. Automatically evict LRU object(s) if cache
 * is full.
 */
void cache_insert (struct cache_header *C,
    char *uri, void *object, size_t object_size);
