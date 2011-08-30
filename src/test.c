
#include "stdint.h"
#include "stddef.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"
#include "math.h"

#include "stdio.h"
#include "sys/time.h"

typedef unsigned int word_t; 

#define SLAB_SIZE (1024*1024)
#define MIN_SLAB_CHUNK_SIZE (100)
#define CHUNK_ALIGN_BYTES (8)

#define CHAR_BIT (8)
#define WORD_SIZE_IN_BITS (sizeof(word_t) * CHAR_BIT)   // in bits
#define WORD_COUNT (SLAB_SIZE / MIN_SLAB_CHUNK_SIZE / WORD_SIZE_IN_BITS)

// TODO: calculate external/internal fragmentation.

typedef struct { 
    word_t words[WORD_COUNT];  
} bitset_t;

typedef struct slab_ctl_t {
    unsigned int nused;
    unsigned int nindex; // index into the slab_ctls. in sync with slabs.
    bitset_t slots;
    struct slab_ctl_t *next;
    struct slab_ctl_t *prev;
    struct cache_t *cache;
} slab_ctl_t;

typedef struct {
    slab_ctl_t *head;
    slab_ctl_t *tail;
} list_t;

typedef struct cache_t {
    unsigned int chunk_size;
    list_t slabs_full;
    list_t slabs_partial;
} cache_t;

typedef struct {
    cache_t *caches;
    unsigned int cache_count;
    slab_ctl_t *slab_ctls;
    unsigned int slabctl_count;
    
    list_t slabs_free;
    
    void *slabs;
} cache_manager_t;

// Globals
static cache_manager_t *cm = NULL;
size_t mem_used = 0;
size_t mem_limit = 0;

void *
malloci(size_t size) 
{
    void *ptr;
    
    ptr = malloc(size);
    memset(ptr, 0x00, size);
    mem_used += size;
    return ptr;
}

inline double
logbn(double base, double x) 
{
    //logb (x) = (loga(x)) / (loga(B))
    return log(x) / log(base);
}

inline unsigned int 
bindex(unsigned int b) { 
    return b / WORD_SIZE_IN_BITS; 
}

inline unsigned int 
boffset(unsigned int b) { 
    return b % WORD_SIZE_IN_BITS; 
}

void
dump_bitset(bitset_t *bts)
{
    int i;
    
    for(i=0; i<WORD_COUNT; i++) {
        printf("word %d is 0x%x\r\n", i, bts->words[i]);
    }
}

void
set_bit(bitset_t *bts, unsigned int b)
{
    assert(b < WORD_COUNT*WORD_SIZE_IN_BITS);
    
    bts->words[bindex(b)] |= (word_t)1 << (boffset(b)); 
}
void 
clear_bit(bitset_t *bts, unsigned int b) { 
    assert(b < WORD_COUNT*WORD_SIZE_IN_BITS);
    
    bts->words[bindex(b)] &= ~((word_t)1 << (boffset(b)));
}

unsigned int 
get_bit(bitset_t *bts, unsigned int b) {
    assert(b < WORD_COUNT*WORD_SIZE_IN_BITS);
 
    return (bts->words[bindex(b)] >> boffset(b)) & 1;
}

// Using ffsll() on an 64-bit machine gains no performance at all.
static int
ff_setbit(bitset_t *bts)
{
    int i, j;
    word_t cword;
  
    for(i=0; i<WORD_COUNT; i++) {
        j = ffs(bts->words[i]);
        if (j) {
            //printf("ff_setbit:%d, %d, %d\r\n", j, (j + (i*WORD_SIZE_IN_BITS))-1,
            //    WORD_SIZE_IN_BITS);
            return (j + (i*WORD_SIZE_IN_BITS))-1;
        }
    }
    
    return -1;
}

slab_ctl_t *
peek(list_t *li) 
{
    return li->head;
}

void
push(list_t *li, slab_ctl_t *item) 
{
    item->next = li->head;
    item->prev = NULL;  
        
    if (li->head) {
        li->head->prev = item;
    } else {
        li->tail = item;
    }
    li->head = item;    
}

slab_ctl_t * 
pop(list_t *li)
{
    slab_ctl_t *result;
    
    result = NULL;
    if (li->head) {
        result = li->head;
        li->head->prev = NULL;
        li->head = li->head->next;
    } 
    
    // check if last item is being popped.
    if (!li->head) {
        li->tail = NULL;
    }
    
    return result;
}

int
rem(list_t *li, slab_ctl_t *item)
{
    slab_ctl_t *cit;
    
    for(cit=li->head; cit != NULL; cit = cit->next) {
        if (cit == item) {
            if (cit == li->head) {
                pop(li);
            } else if (cit == li->tail) {
                
                // we SHALL have tail->prev here. Because if not, then
                // li->head == li->tail and if so, above if (cit == li->head)
                // will capture that.
                assert(li->tail->prev != NULL);
                
                li->tail->prev->next = NULL;
                li->tail = li->tail->prev;
                }
            } else {
                cit->prev->next = cit->next;
                cit->next->prev = cit->prev;
            }
            return 1;
        }
    
    return 0;
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
    
    assert(cm == NULL);
    assert(mem_used == 0);
    assert(mem_limit == 0);
    
    // initialize globals
    mem_used = 0;
    mem_limit = memory_limit * 1024*1024; // memory_limit is in MB
    cm = malloci(sizeof(cache_manager_t));
    
    // cache_count is calculated by starting from the MIN_SLAB_CHUNK_SIZE and
    // multiplying it with chunk_size_factor for every iteration till we reach
    // SLAB_SIZE. This idea is being used on memcached() and proved to be well
    // on real-world.
    cm->cache_count = (unsigned int)ceil(logbn(chunk_size_factor, 
        SLAB_SIZE/MIN_SLAB_CHUNK_SIZE));
    //fprintf(stderr, "ccount:%u\r\n", cm->cache_count);
    
    // alloc&initialize caches
    cm->caches = malloci(sizeof(cache_t)*cm->cache_count);
    for(i=0,size=MIN_SLAB_CHUNK_SIZE; i < cm->cache_count; size*=chunk_size_factor, i++)
    {
        size = align_bytes(size);        
        cm->caches[i].chunk_size = size;
    }
    
    // calculate remaining memory for slabs.
    cm->slabctl_count = mem_limit / (SLAB_SIZE+sizeof(slab_ctl_t));
    if (cm->slabctl_count <= 1){
        fprintf(stderr, "not enough mem to create a slab\r\n");
        return 0;
    }
    
    // alloc&initialize slab_ctl and slabs
    cm->slab_ctls = malloci(sizeof(slab_ctl_t)*cm->slabctl_count);
    cm->slabs = malloci(SLAB_SIZE*cm->slabctl_count);
    cm->slabs_free.head = cm->slab_ctls;
    cm->slabs_free.tail = &cm->slab_ctls[cm->slabctl_count-1];    
    prev_slab = NULL;
    for(i=0;i < cm->slabctl_count; i++) {
        cm->slab_ctls[i].prev = prev_slab;
        if (i == cm->slabctl_count-1) { // last element?
            cm->slab_ctls[i].next = NULL;
        } else {
            cm->slab_ctls[i].next = &cm->slab_ctls[i+1];
        }
        cm->slab_ctls[i].nindex = i;
        
        // setbit indicates free slot. This is because we have a builtin ffs()
        // routine to find the first set bit in a desired integral type. The latter
        // ffz() have little examples and I am not an expert there.
        memset(&cm->slab_ctls[i].slots, 0xFF, sizeof(word_t)*WORD_COUNT);
        
        prev_slab = &cm->slab_ctls[i];
    }
    
    return 1;
}

void *
scmalloc(size_t size)
{
    unsigned int i, ffindex;
    cache_t *ccache;
    void *result;
    slab_ctl_t *cslab;
    
    // find relevant cache 
    // TODO: change with binsearch later, iequalities can also be efficient.
    // O(log((n)) + O(1) time
    for(i = 0; i < cm->cache_count; i++) {
        if (size <= cm->caches[i].chunk_size) {
            ccache = &cm->caches[i];
            break;
        }
    }
    
    // need to allocate a slab_ctl?
    cslab = peek(&ccache->slabs_partial);
    if (cslab == NULL) {
        cslab = pop(&cm->slabs_free);
        if (cslab == NULL) {
            // TODO: log err, NO_MEM.
            return NULL;
        }
        push(&ccache->slabs_partial, cslab);
        cslab->cache = ccache;
    } 
    
    // must be equal, axtra validation 
    assert(cslab->cache == ccache);
    
    if (++cslab->nused == (SLAB_SIZE /ccache->chunk_size)) {
        cslab = pop(&ccache->slabs_partial);
        push(&ccache->slabs_full, cslab);
    } 
    
    ffindex = ff_setbit(&cslab->slots);
    assert(ffindex != -1); // we take cslab from partial, so ffindex should be valid.
    clear_bit(&cslab->slots, ffindex);
    
    result = cm->slabs + cslab->nindex * SLAB_SIZE;
    result += ccache->chunk_size * ffindex;
    
    mem_used += ccache->chunk_size;
    
    //fprintf(stderr, "alloc slab nindex:%u  %u slot is alloc'd\r\n", 
    //   cslab->nindex, ffindex);
    
    /*
    fprintf(stderr, "malloc request:%u"
        " cache->size:%u"
        " ptr:%p"
        "\r\n", 
        size, 
        ccache->chunk_size,
        result
        );
    */
    return result;
}

void
scfree(void *ptr)
{
    unsigned int sidx, cidx;
    ptrdiff_t pdiff;
    slab_ctl_t *cslab;
    int res;
    
    pdiff = ptr - cm->slabs;    
    
    // ptr shall be in valid memory
    if (pdiff > cm->slabctl_count*SLAB_SIZE) {
        fprintf(stderr, "Invalid ptr.(%p)", ptr);
        return;
    }
    
    sidx = pdiff / SLAB_SIZE;    
    cslab = &cm->slab_ctls[sidx];
    cidx = (pdiff % SLAB_SIZE) / cslab->cache->chunk_size;
    
    // check if ptr is really allocated?
    if (get_bit(&cslab->slots, cidx) != 0) {
        fprintf(stderr, "ptr not allocated.(%p)", ptr);
        return;
    }
    
    set_bit(&cslab->slots, cidx);
    if (--cslab->nused == 0) {
        // we shall have no unfree chunk here
        // TODO: Implement assert(ff_setbit(&cslab->slots) == -1);
        
        if (!rem(&cslab->cache->slabs_partial, cslab)) {
            res = rem(&cslab->cache->slabs_full, cslab);
            assert(res == 1); // somebody must own the slab.
        }
        push(&cm->slabs_free, cslab);
    } else {
        // TODO: move closer to head for efficiency?
    }
    
    mem_used -= cslab->cache->chunk_size;
}

void 
test_bit_set(void)
{
    bitset_t y;
    
    // for this test to work this assertion must be true.
    // change below compile-time params accordingly.
    assert(69 < WORD_SIZE_IN_BITS * WORD_COUNT);
    
    //memset(&y, 0x00, sizeof(bitset_t));    
    //set_bit(&y, 96);
    //dump_bitset(&y);
    //ff_setbit(&y);
    
    //return;
    memset(&y, 0x00, sizeof(bitset_t));    
    assert(get_bit(&y, 69) == 0);
    set_bit(&y, 69);
    assert(get_bit(&y, 69) == 1);
    
    set_bit(&y, 64);   
    assert(ff_setbit(&y) == 64);
    
    memset(&y, 0x00, sizeof(bitset_t));
    assert(ff_setbit(&y) == -1);
    set_bit(&y, 67);
    assert(ff_setbit(&y) == 67);
    assert(get_bit(&y, 67) == 1);
    clear_bit(&y, 67);
    assert(get_bit(&y, 67) == 0);
    
    memset(&y, 0x00, sizeof(bitset_t));
    set_bit(&y, 57);
    set_bit(&y, 58);
    assert(ff_setbit(&y) == 57);
    set_bit(&y, 56);
    assert(ff_setbit(&y) == 56);
    clear_bit(&y, 57);
    clear_bit(&y, 58);
    clear_bit(&y, 56);
    set_bit(&y, 60);
    assert(ff_setbit(&y) == 60);
    clear_bit(&y, 60);
    assert(ff_setbit(&y) == -1);
    
    fprintf(stderr, "[+]    test_bit_set. (ok)\r\n");
}

long long
tickcount(void)
{
    struct timeval tv;
    long long rc;

    gettimeofday(&tv, (struct timezone *)NULL);

    rc = tv.tv_sec;
    rc = rc * 1000000 + tv.tv_usec;
    return rc;
}

void
test_slab_allocator(void)
{
    void *tmp;    
    unsigned d;
    long long t0;
    
    init_cache_manager(5, 1.25); 
    
    t0 = tickcount();
    for(d=0;d<10000000;d++) {
        tmp = scmalloc(50);
        scfree(tmp);
        //tmp = malloc(50);
        //free(tmp);
    }
    
        //tmp = malloc(50);
    printf("Elapsed:%0.12f \r\n", (tickcount()-t0)*0.000001);

    //scfree(tmp);
    
    fprintf(stderr, "mem_limit:%u, mem_used:%u\r\n", mem_limit, mem_used);
    fprintf(stderr, "mem_avail_for_slabs:%u\r\n", mem_limit-mem_used);
      
}



int
main(void)
{
    
    test_bit_set();
    test_slab_allocator();
    
    return 0;
}