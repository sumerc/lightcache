
#include "stdint.h"
#include "stddef.h"
#include "stdlib.h"
#include "string.h"

#include "stdio.h"

// 32 or 64 does not matter from performance perspective. Also endianness 
// does not affect bit operations.
typedef int word_t; 

#define SLAB_SIZE (1024*1024)
#define MIN_SLAB_CHUNK_SIZE (100)
#define CHUNK_ALIGN_BYTES (8)
#define BITSET_SIZE (SLAB_SIZE / MIN_SLAB_CHUNK_SIZE / (sizeof(word_t) * 8))

typedef struct { 
    word_t words[BITSET_SIZE];  
} bitset_t;

typedef struct slab_ctl_t {
    unsigned int nused;
    unsigned int nindex; // index into the slab_ctls. in sync with slabs.
    bitset_t slots;
    struct slab_ctl_t *next;
    struct slab_ctl_t *prev;
    struct cache_t *cache;
} slab_ctl_t;

typedef struct cache_t {
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
    
    void *slabs;
} cache_manager_t;

// Globals
static cache_manager_t *cm;
size_t mem_used;
size_t mem_limit;

static inline unsigned int
ffsetbit(bitset_t *bts)
{
    return 1;
}

static inline unsigned int
setbit(bitset_t *bts, unsigned int index, int bit)
{
    return 1;
}


// TODO: do not make func. if only called once.
static inline size_t
align_bytes(size_t size)
{
    size_t sz;
    
    sz = size % CHUNK_ALIGN_BYTES;
    if (sz)
        size += CHUNK_ALIGN_BYTES - sz;    
    return size;
}

static int
init_cache_manager(size_t memory_limit, double chunk_size_factor)
{
    unsigned int size,i;
    slab_ctl_t *prev_slab,*cslab;
    
    mem_used = 0;
    mem_limit = memory_limit * 1024*1024; // memory_limit is in MB
    
    cm = malloc(sizeof(cache_manager_t));
    memset(cm, 0, sizeof(cache_manager_t));
    mem_used += sizeof(cache_manager_t);
    
    for(size=MIN_SLAB_CHUNK_SIZE; size < SLAB_SIZE; size*=chunk_size_factor)
    {
        cm->cache_count++;
    }
    
    
    cm->caches = malloc(sizeof(cache_t)*cm->cache_count);
    memset(cm->caches, 0, sizeof(cache_t)*cm->cache_count);
    mem_used += sizeof(cache_t)*cm->cache_count;
    
    for(i=0,size=MIN_SLAB_CHUNK_SIZE; size < SLAB_SIZE; size*=chunk_size_factor, i++)
    {
        size = align_bytes(size);
        
        cm->caches[i].chunk_size = size;
        
        fprintf(stderr, "cache %u) size:%u\r\n", i, 
            cm->caches[i].chunk_size);
    }
    
    fprintf(stderr, "memory_limit:%u, mem_used:%u\r\n", mem_limit, mem_used);
    
    // allocate slab_ctl and slabs
    cm->slabctl_count = mem_limit / (SLAB_SIZE+sizeof(slab_ctl_t));
    if (cm->slabctl_count <= 1){
        // TODO: log err.
        fprintf(stderr, "not enough mem to create a slab\r\n");
        return 0;
    }
    cm->slab_ctls = malloc(sizeof(slab_ctl_t)*cm->slabctl_count);
    memset(cm->slab_ctls, 0, sizeof(slab_ctl_t)*cm->slabctl_count);
    mem_used += sizeof(slab_ctl_t)*cm->slabctl_count;
    cm->slabs = malloc(SLAB_SIZE*cm->slabctl_count);
    memset(cm->slabs, 0, SLAB_SIZE*cm->slabctl_count);
    
    // initialize slab_ctl structures
    cm->slabs_free_head = cm->slab_ctls;
    cm->slabs_free_tail = &cm->slab_ctls[cm->slabctl_count-1];
    cm->slabs_free_tail->prev = &cm->slab_ctls[cm->slabctl_count-2];
    cm->slabs_free_tail->next = NULL;
    cm->slabs_free_tail->nindex = cm->slabctl_count-1;
    
    prev_slab = NULL;
    for(i=0;i < cm->slabctl_count-1; i++) {
        cm->slab_ctls[i].prev = prev_slab;
        cm->slab_ctls[i].next = &cm->slab_ctls[i+1];    
        cm->slab_ctls[i].nindex = i;
        
        prev_slab = &cm->slab_ctls[i];
    }
    
    for(cslab = cm->slabs_free_head; cslab != NULL; cslab = cslab->next) {  
        fprintf(stderr, "nindex of the slab:%u\r\n", cslab->nindex);  
    }
    
    for(cslab = cm->slabs_free_tail; cslab != NULL; cslab = cslab->prev) {  
        fprintf(stderr, "nindex of the slab:%u\r\n", cslab->nindex);  
    }
    
    fprintf(stderr, "mem_limit:%u, mem_used:%u\r\n", mem_limit, mem_used);
    fprintf(stderr, "mem_avail_for_slabs:%u\r\n", mem_limit-mem_used);
      
    return 1;
}

void *
scmalloc(size_t size)
{
    unsigned int i, ffindex;
    cache_t *ccache;
    void *result;
    slab_ctl_t *cslab;
    
    // find relevant cache, TODO: maybe change with binsearch later on.
    for(i = 0; i < cm->cache_count; i++) {
        if (size <= cm->caches[i].chunk_size) {
            ccache = &cm->caches[i];
            break;
        }
    }
    
    // need to allocate a slab_ctl?
    if (ccache->slabs_partial_head == NULL) {
        ;// TODO: move to our partial from free, return NO_MEM if no free found.
        ccache->slabs_partial_head = cm->slabs_free_head;
        cslab = ccache->slabs_partial_head;
    }
    
    cslab->cache = ccache;
    
    ffindex = ffsetbit(&cslab->slots);
    if (++cslab->nused == ccache->chunk_count) {
        ; // TODO: move to full from partial
    } 
    setbit(&cslab->slots, ffindex, 1);
    
    result = cm->slabs + cslab->nindex * SLAB_SIZE;
    result += ccache->chunk_size * ffindex;
    
    mem_used += ccache->chunk_size;
    
    fprintf(stderr, "malloc request:%u"
        " cache->size:%u"
        " ptr:%p"
        "\r\n", 
        size, 
        ccache->chunk_size,
        result
        );
    
    return result;
}

void
scfree(void *ptr)
{
    unsigned int sidx, cidx;
    ptrdiff_t pdiff;
    
    pdiff = ptr - cm->slabs;    
    sidx = pdiff / SLAB_SIZE;
    cidx = (pdiff % SLAB_SIZE) / cm->slab_ctls[sidx].cache->chunk_size;
    
    fprintf(stderr, "sidx:%u, cidx:%u\r\n", sidx, cidx);
    setbit(&cm->slab_ctls[sidx].slots, cidx, 0);
    
    if (--cm->slab_ctls[sidx].nused == 0) {
        // TODO: move from partial or full to free.
    }
}

int
main(void)
{
    void *tmp;
    
    init_cache_manager(5, 1.25);
    
    tmp = scmalloc(50);
    
    scfree(tmp);
    
    return 0;
}