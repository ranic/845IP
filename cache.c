/*
 * cache.c
 */

#include "csapp.h"
#include "cache.h"
#include "contracts.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/*
 * struct of a cache header.
 */
struct cache_header {
    struct cache_block *start;
    struct cache_block *end;
    int cache_size;
    int cache_block_num;
    sem_t lock;
};

/*
 * cache_init - initialize a cache.
 */
struct cache_header *cache_init () {
    struct cache_header *new = Malloc (sizeof(struct cache_header));
    struct cache_block *dummy = Malloc (sizeof(struct cache_block));
    new->start = dummy;
    new->end = dummy;
    new->cache_size = 0;
    new->cache_block_num = 0;
    pthread_mutex_init(&(new->lock), NULL);
    return new;
}

/*
 * cache_add_to_end - add a cache_block at the end of the link list.
 */
void cache_add_to_end (struct cache_header *C, struct cache_block *cb) {
    REQUIRES (C != NULL);
    REQUIRES (C->end != NULL);
    REQUIRES (cb != NULL);
    printf("In cache add to end\n");
    C->cache_size += cb->object_size;
    C->cache_block_num += 1;

    C->end->object_name = cb->object_name;
    C->end->object = cb->object;
    C->end->object_size = cb->object_size;
    
    /* the original cb becomes a dummy node */
    C->end->next = cb;
    C->end = cb;
    return;
}

/*
 * cache_evict - evict an least recently used block from a cache.
 */
void cache_evict (struct cache_header *C) {
    REQUIRES(C != NULL);
    REQUIRES(C->cache_block_num != 0);
    printf("In cache evict\n");
    struct cache_block *start = C->start;
    C->cache_size -= start->object_size;
    C->cache_block_num -= 1;
    
    ASSERT (0 <= C->cache_size && C->cache_size <= MAX_CACHE_SIZE);
    ASSERT (C->cache_block_num >= 0);

    if (dlclose(start->object) < 0) {
        fprintf(stderr, "%s\n", dlerror());
    }

    C->start = C->start->next;
    Free(start->object);
    Free(start->object_name);
    Free(start);
    return;
}

/*
 * cache_delete - delete a block from a cache.
 */
void cache_delete (struct cache_header *C, struct cache_block *cb) {
    REQUIRES (C != NULL);
    REQUIRES (cb != NULL);
    printf("In cache delete\n");
    /* if the node is the last node */
    if (cb->next == C->end) {
        if (C->cache_block_num == 1) {
            // In our implementation, this will happen only if the cache
            // consists of exactly one block.
            C->end = C->start;
            C->cache_block_num = 0;
            ASSERT (C->cache_size == cb->object_size);
            C->cache_size = 0;
        }
        else {
            fprintf(stderr, "cache_delete invariant error.\n");
        }
    }
    /* the node is somewhere in the middle */
    else {
        C->cache_block_num -= 1;
        C->cache_size -= cb->object_size;
        ASSERT (0 <= C->cache_size && C->cache_size <= MAX_CACHE_SIZE);
        ASSERT (C->cache_block_num >= 0);
        cb->object_name = cb->next->object_name;
        cb->object_size = cb->next->object_size;
        cb->object = cb->next->object;
        cb->next = cb->next->next;
    }
    return;
}

/*
 * cache_find - return a pointer to a cache_block whose object_name
 * is the same as uri.
 */
struct cache_block *cache_find (struct cache_header *C, char *uri) {
    REQUIRES (C != NULL);
    printf("In cache find\n");
    pthread_mutex_lock(&(C->lock));

    struct cache_block *ptr = C->start;
    while (ptr != C->end) {
        /* found the object */
        if (!strcmp(uri, ptr->object_name)) {
            /* if the block is already a LRU */
            if (ptr->next == C->end) {
                pthread_mutex_unlock(&(C->lock));
                return ptr;
            }
            /* otherwise, move it to the end and return */
            else {
                struct cache_block *old = Malloc (sizeof(struct cache_block));
                old->object_name = ptr->object_name;
                old->object_size = ptr->object_size;
                old->object = ptr->object;
                old->next = ptr->next;

                cache_delete(C, ptr);
                cache_add_to_end(C, old);
                pthread_mutex_unlock(&(C->lock));
                return old;
            }

        }
        ptr = ptr->next;
    }

    /* not found */
    pthread_mutex_unlock(&(C->lock));
    return NULL;
}

/*
 * cache_insert - insert an cache_block into a cache.
 */
void cache_insert (struct cache_header *C, char *name, void *object,
                   size_t object_size) {
    REQUIRES (C != NULL);
    printf("In cache insert\n");
    pthread_mutex_lock(&(C->lock));

    /* if the object is too large, simple ignore it */
    //if (object_size > MAX_OBJECT_SIZE) return;

    /* if the cache is full, evict blocks until the object can be fitted in */
    while (object_size + C->cache_size > MAX_CACHE_SIZE) {
        cache_evict(C);
    }

    /* copy the name */
    size_t object_name_size = strlen(name) + 1;
    char *copied_object_name = Malloc(object_name_size);
    memcpy(copied_object_name, name, object_name_size);

    /* create a new block and add into the cache */
    struct cache_block *new = Malloc(sizeof(struct cache_block));
    new->object_name = copied_object_name;
    new->object_size = object_size;
    new->object = object;
    cache_add_to_end(C, new);

    pthread_mutex_unlock(&C->lock);
    return;
}

