#include <stdlib.h>
#include <string.h>
#include "cache.h"
#include "csapp.h"

#define line_size sizeof(proxy_line)

/*The cache of the proxy server is handled as a linkedlist. 
It follows the eviction policy that approximates a least-recently-used (LRU) eviction policy.*/


static proxy_line * last = NULL;
static proxy_line *dummynode = NULL;
static size_t total_size = 0;

/*initialize the cache.*/
static void cacheline_init() {
    dummynode = (proxy_line *)malloc(sizeof(proxy_line));
    dummynode -> size = 0;
    dummynode -> age = 0;
    dummynode -> buf = NULL;
    dummynode -> name = NULL;
    dummynode -> next = NULL;
    last = dummynode;
}
/*proxy_line function is used to find the cache to be evicted when there isn't
enough space to allocate a new cacheline. Return the proxy_line pointer if it 
successfully find one to be evicted, or it will return NULL.*/
static proxy_line *cache_evict(size_t size) {
    int max_age = 0;
    proxy_line *toevict = NULL;
    proxy_line *p = dummynode -> next;
    while (p != NULL) {
        if (p -> size < size) {
            continue;
        }

        if (p -> age > max_age) {
            max_age = p -> age;
            toevict = p;
        }

        p = p -> next;
    }

    return toevict;
}

/*The updateage function is used to update the age of the previous cacheline
when a new object is inserted.*/
static void updateage() {
    proxy_line *p = dummynode;
    p = p -> next;
    while (p != NULL) {
        (p -> age) ++;
        p = p -> next;
    }
}

/*insert function is used to insert a new element to the cache of the proxy server.*/
void insert(char *buf, char *name) {
    proxy_line *cacheline;
    size_t buf_size = strlen(buf) + 1;

    if (buf_size > MAX_OBJECT_SIZE) {
        return;
    }

    if (dummynode == NULL) {
        cacheline_init();
    }

    updateage();
    if (total_size + buf_size <= MAX_CACHE_SIZE) {
        cacheline = (proxy_line *)malloc(sizeof(proxy_line));

        last -> next = cacheline;
        last = cacheline;
        cacheline -> next = NULL;
    } else {
        cacheline = cache_evict(buf_size);
        Free(cacheline -> buf);
        Free(cacheline -> name);
        total_size  -= (cacheline -> size);
    }

    cacheline -> size = buf_size;
    cacheline -> age = 0;
    cacheline -> name = (char *)malloc(strlen(name) + 1);
    strcpy(cacheline -> name, name);
    cacheline -> buf = (char *)malloc(cacheline -> size);
    strcpy(cacheline -> buf, buf);

    total_size += buf_size;
}

/*Search in the cache to find if there is a hit. If so, the function return a pointer
pointing to the hited cacheline, or it will return NULL.*/
proxy_line* search_cache(char *name) {
    if (dummynode == NULL) {
        cacheline_init();
    }

    proxy_line *p = dummynode -> next;
    while (p != NULL) {
        if (strcmp(p -> name, name) == 0) {
        	p -> age = 0;
            return p;
        }
        p = p -> next;
    }

    return NULL;
}
