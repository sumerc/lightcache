
#include "stdint.h"
#include "stddef.h"
#include "stdlib.h"
#include "string.h"

#include "stdio.h"

typedef uint32_t word_t; // todo: change for 64 bit?

#define SLAB_SIZE (1024*1024)
#define MIN_SLAB_CHUNK_SIZE 100
#define CHUNK_ALIGN_BYTES 8
#define MAX_BITSET_SIZE (SLAB_SIZE / MIN_SLAB_CHUNK_SIZE / (sizeof(word_t)*8))

typedef struct { 
    word_t words[MAX_BITSET_SIZE];  
} bitset_t;

typedef struct slab_ctl_s {
    unsigned int nused;
    bitset_t *slots;
    struct slab_ctl_s *next;
    struct slab_ctl_s *prev;
} slab_ctl_t;

typedef struct {
    unsigned int chunk_size;    
    unsigned int chunk_count;
    slab_ctl_t *slabs_full_head;
    slab_ctl_t *slabs_full_tail;
    slab_ctl_t *slabs_partial_head;
    slab_ctl_t *slabs_partial_tail;
} cache_t;

typedef struct {
    cache_t *caches;
    unsigned int cache_count;
    slab_ctl_t *slab_ctls;
    unsigned int slabctl_count;
    
    slab_ctl_t *slabs_free_head;
    slab_ctl_t *slabs_free_tail;
} cache_manager_t;

static cache_manager_t *cm;

static int
init_cache_manager(size_t memory_limit, double chunk_size_factor)
{
    unsigned int size,i;
    size_t mem_used;
    
    mem_used = 0;
    memory_limit *= 1024*1024; // memory_limit is in MB
    
    cm = malloc(sizeof(cache_manager_t));
    memset(cm, 0, sizeof(cache_manager_t));
    mem_used += sizeof(cache_manager_t);
    
    for(size=MIN_SLAB_CHUNK_SIZE; size < SLAB_SIZE; size*=chunk_size_factor)
    {
        cm->cache_count++;
    }
    
    
    cm->caches = malloc(sizeof(cache_t)*cm->cache_count);
    mem_used += sizeof(cache_t)*cm->cache_count;
    
    for(i=0,size=MIN_SLAB_CHUNK_SIZE; size < SLAB_SIZE; size*=chunk_size_factor, i++)
    {
        if (size % CHUNK_ALIGN_BYTES)
            size += CHUNK_ALIGN_BYTES - (size % CHUNK_ALIGN_BYTES);
        cm->caches[i].chunk_size = size;
        
        fprintf(stderr, "cache %u) size:%u\r\n", i, 
            cm->caches[i].chunk_size);
    }
    
    fprintf(stderr, "memory_limit:%u, mem_used:%u\r\n", memory_limit, mem_used);
    
    // calculate the remaining slab count after all the slab allocator metadata 
    // structures.
    cm->slabctl_count = memory_limit / (SLAB_SIZE+sizeof(slab_ctl_t));
    
    fprintf(stderr, "slab_available:%u\r\n", cm->slabctl_count);
    
    cm->slab_ctls = malloc(sizeof(slab_ctl_t)*cm->slabctl_count);
    mem_used += sizeof(slab_ctl_t)*cm->slabctl_count;
    
    
    cm->slabs_free_head = cm->slab_ctls;
    cm->slabs_free_tail = &cm->slab_ctls[cm->slabctl_count-1];
    
    fprintf(stderr, "memory_limit:%u, mem_used:%u\r\n", memory_limit, mem_used);
    
    fprintf(stderr, "mem_avail_for_slabs:%u\r\n", memory_limit-mem_used);
    
    return 1;
}

static cache_t*
size_to_cache(size_t size)
{
    unsigned int i;
    
    // TODO: Make below O(logn) via binary search? 
    
    for(i=0; i < cm->cache_count; i++) {
        if (cm->caches[i]->chunk_size >= size) {    
            return cm->caches[i];
        }
    }
}


void *
scmalloc(size_t size)
{
    cache_t *ca;
    
    ca = size_to_cache(size)
    if (ca == NULL){
        // TODO: log err?
        return NULL;
    }
    
    if (ca->slabs_partial_head == NULL){
        ca->slabs_partial_head = cm->slabs_free_head;
    }
    
    return NULL;
}

void
scfree(void *ptr)
{
    return;
}


int
main(void)
{
    void *p;
    
    init_cache_manager(5, 1.25);
    
    p = scmalloc(5);
    scfree(p);
    
    return 0;
}