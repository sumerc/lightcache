#include "slab.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"
#include "math.h"
#include "limits.h"
#include "stdio.h"

#define WORD_SIZE_IN_BITS (sizeof(word_t) * CHAR_BIT)   // in bits
#define WORD_COUNT ((SLAB_SIZE / MIN_SLAB_CHUNK_SIZE / WORD_SIZE_IN_BITS)+1)

#define SLAB_INIT_MALLOC_ERR "slab allocator initialization failed: malloc failed.\r\n"
#define SLAB_ALREADY_INIT_ERR "slab allocator initialization failed: already initialized.\r\n"

typedef uint64_t word_t;
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
    unsigned int chunk_count_perslab; // for efficiency
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
slab_stats_t slab_stats;

static void *malloci(size_t size)
{
    void *ptr;
    size_t real_size;

    real_size = size+sizeof(uint64_t);
    
    // TODO: check for mem limit
    
    ptr = malloc(real_size);
    if (!ptr) {
        return NULL;
    }
    memset(ptr, 0x00, real_size);
    *(uint64_t *)ptr = real_size;
    slab_stats.mem_mallocd += real_size;
    return (char *)ptr+sizeof(uint64_t);
}

static void freei(void *ptr)
{
    char *real_ptr;
    assert(ptr != NULL);
    
    real_ptr = (char *)ptr - sizeof(uint64_t);
    slab_stats.mem_mallocd -= *(uint64_t *)(real_ptr);
    free(real_ptr);
}

static inline unsigned int bindex(unsigned int b)
{
    return b / WORD_SIZE_IN_BITS;
}

static inline unsigned int bloffset(unsigned int b)
{
    return (b % WORD_SIZE_IN_BITS);
}

#if 0
static void dump_bitset(bitset_t *bts)
{
    int i,j;

    for(i=0; i<WORD_COUNT; i++) {
        for(j=0; j<sizeof(word_t); j+=sizeof(unsigned int)) {
            printf("word %d is 0x%x\r\n", i, *(unsigned int *)(&bts->words[i]+j));
        }
    }
}
#endif

static void set_bit(bitset_t *bts, unsigned int b)
{
    assert(b < WORD_COUNT*WORD_SIZE_IN_BITS);

    bts->words[bindex(b)] |= (word_t)1 << (bloffset(b));
}
static void clear_bit(bitset_t *bts, unsigned int b)
{
    assert(b < WORD_COUNT*WORD_SIZE_IN_BITS);

    bts->words[bindex(b)] &= ~((word_t)1 << (bloffset(b)));
}

static unsigned int get_bit(bitset_t *bts, unsigned int b)
{
    assert(b < WORD_COUNT*WORD_SIZE_IN_BITS);

    return (bts->words[bindex(b)] >> (bloffset(b))) & 1;
}

// Among other tested alternatives this is the fastest. The only problem
// is we may have a cache miss because of the table usage. But this is a
// O(1) operation as can be seen and very efficient.
// http://keithandkatie.com/keith/papers/debruijn.pdf can be read for
// more info. Paper indicates 64 bit mul. can be slower than 32 bit, however
// for our case with many words, it seems using 64 bit is roughly %20 faster in
// 64-bit machines. Using below with 32 bit, however, contradicting with the paper
// have roughly same performance with the 32-bit version.
static int nlz64(register uint64_t x)
{
    static const unsigned int debruij_tab[64] = {
        0,  1,  2, 53,  3,  7, 54, 27,
        4, 38, 41,  8, 34, 55, 48, 28,
        62,  5, 39, 46, 44, 42, 22,  9,
        24, 35, 59, 56, 49, 18, 29, 11,
        63, 52,  6, 26, 37, 40, 33, 47,
        61, 45, 43, 21, 23, 58, 17, 10,
        51, 25, 36, 32, 60, 20, 57, 16,
        50, 31, 19, 15, 30, 14, 13, 12,
    };
    return debruij_tab[((x&-x)*0x022FDD63CC95386DU) >> 58];
}

static int ff_setbit(bitset_t *bts)
{
    unsigned int i;
    int j;
    word_t w;

    for(i=0; i<WORD_COUNT; i++) {
        w = bts->words[i];
        if(w) {
            j = nlz64(w);
            return (j + (i*WORD_SIZE_IN_BITS));
        }
    }

    return -1;
}

static slab_ctl_t *peek(list_t *li)
{
    return li->head;
}

static void push(list_t *li, slab_ctl_t *item)
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

static slab_ctl_t *pop(list_t *li)
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

static int rem(list_t *li, slab_ctl_t *item)
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
            } else {
                cit->prev->next = cit->next;
                cit->next->prev = cit->prev;
            }
            return 1;
        }
    }

    return 0;
}

static int rem_and_push(list_t *src, list_t *dest, slab_ctl_t *item)
{
    if(rem(src, item)) {
        push(dest, item);
        return 1;
    }

    return 0;
}

static int pop_and_push(list_t *src, list_t *dest)
{
    slab_ctl_t *item;

    item = pop(src);
    if(item) {
        push(dest, item);
        return 1;
    }

    return 0;
}

static inline double logbn(double base, double x)
{
    return log(x) / log(base);
}

// A binary search like routine for ceiling the requested size to a cache with proper size.
// Implemented to reduce the mapping complexity to O(logn) in scmalloc()'s.
static cache_t *size_to_cache(cache_t *arr, unsigned int arr_size, unsigned int key)
{
    int l, m, r;
    unsigned int msize;

    // validate params
    if ((arr == NULL) || (arr_size >= INT_MAX) || (arr_size < 1)) {
        return NULL;
    }

    l = m = 0;
    r = arr_size-1;
    while(l < r) {
        m = (l+r) / 2;
        msize = arr[m].chunk_size;
        if (key > msize) {
            l = m+1;
        } else if (key < msize) {
            r = m-1;
        } else {
            return &arr[m];
        }
    }

    // will ceil the inequality. Either r or r+1 will hold the ceil value
    // according to where we approach the inequality from. This is not very easy
    // to understand. So below will loop at most 2 times.
    if (r < 0) { // r can be zero because of m-1.
        r = 0;
    }
    if (key <= arr[r].chunk_size) {
        return &arr[r];
    } else if ((unsigned int)r < arr_size-1) {
        if (key <= arr[r+1].chunk_size) {
            return &arr[r+1];
        }
    }

    return NULL;
}

static void deinit_cache_manager(void)
{
    if (cm == NULL) {
        return;
    }
    if (cm->caches != NULL) {
        freei(cm->caches);
    }
    if (cm->slab_ctls != NULL) {
        freei(cm->slab_ctls);
    }
    if (cm->slabs != NULL) {
        freei(cm->slabs);
    }
    freei(cm);

    cm = NULL;
    slab_stats.mem_used_metadata = slab_stats.mem_used = slab_stats.mem_limit = 0;
    slab_stats.cache_count = 0;
    slab_stats.slab_count = 0;
        
    assert(slab_stats.mem_mallocd == 0);// all real-mallocd chunks shall be freed here.
}

int init_cache_manager(size_t memory_limit, double chunk_size_factor)
{
    unsigned int size,i;
    slab_ctl_t *prev_slab;
    
    if (cm != NULL) {
        fprintf(stderr, SLAB_ALREADY_INIT_ERR);
        goto err;
    }

    // initialize globals
    slab_stats.mem_limit = memory_limit*1024*1024; // memory_limit is in MB
    cm = malloci(sizeof(cache_manager_t));
    if (!cm) {
        fprintf(stderr, SLAB_INIT_MALLOC_ERR);
        goto err;
    }

    // cache_count is calculated by starting from the MIN_SLAB_CHUNK_SIZE and
    // multiplying it with chunk_size_factor for every iteration till we reach
    // SLAB_SIZE. This idea is being used on memcached() and proved to be well
    // on real-world.
    // TODO: !!!check below cannot return below 0.
    cm->cache_count = (unsigned int)floor(logbn(chunk_size_factor,
                                         SLAB_SIZE/MIN_SLAB_CHUNK_SIZE))-1;
    slab_stats.cache_count = cm->cache_count;
    
    // alloc/initialize caches
    cm->caches = malloci(sizeof(cache_t)*cm->cache_count);
    if (!cm->caches) {
        fprintf(stderr, SLAB_INIT_MALLOC_ERR);
        goto err;
    }

    for(i=0,size=MIN_SLAB_CHUNK_SIZE; i < cm->cache_count; size*=chunk_size_factor, i++) {
        if (size % CHUNK_ALIGN_BYTES) {
            size += CHUNK_ALIGN_BYTES - (size % CHUNK_ALIGN_BYTES);
        }
        cm->caches[i].chunk_size = size;
        cm->caches[i].chunk_count_perslab = SLAB_SIZE / size;
    }

    // calculate remaining memory for slabs. sizeof(uint64_t) is the bytes
    // allocated at every malloced chunk. Add that, too.
    cm->slabctl_count = (slab_stats.mem_limit-slab_stats.mem_mallocd-(2*sizeof(uint64_t))) /
                        (SLAB_SIZE+sizeof(slab_ctl_t));
    if (cm->slabctl_count <= 1) {        
        fprintf(stderr, SLAB_INIT_MALLOC_ERR);
        goto err;
    }
    slab_stats.slab_count = cm->slabctl_count;
    
    // alloc/initialize slab_ctl and slabs
    cm->slab_ctls = malloci(sizeof(slab_ctl_t)*cm->slabctl_count);
    if (!cm->slab_ctls) {
        fprintf(stderr, SLAB_INIT_MALLOC_ERR);
        goto err;
    }
    // all metadata is shall be allocated here.
    slab_stats.mem_used_metadata = slab_stats.mem_mallocd;
    
    cm->slabs = malloci(SLAB_SIZE*cm->slabctl_count);
    if (!cm->slabs) {
        fprintf(stderr, SLAB_INIT_MALLOC_ERR);
        goto err;
    }
    cm->slabs_free.head = cm->slab_ctls;
    cm->slabs_free.tail = &cm->slab_ctls[cm->slabctl_count-1];
    prev_slab = NULL;
    for(i=0; i < cm->slabctl_count; i++) {
        cm->slab_ctls[i].prev = prev_slab;
        if (i == cm->slabctl_count-1) {
            cm->slab_ctls[i].next = NULL;
        } else {
            cm->slab_ctls[i].next = &cm->slab_ctls[i+1];
        }
        cm->slab_ctls[i].nindex = i;
        prev_slab = &cm->slab_ctls[i];

        // setbit indicates free slot. so set all.
        memset(&cm->slab_ctls[i].slots, 0xFF, sizeof(word_t)*WORD_COUNT);
    }

    // mem_alloc shall always be smaller than mem_limit
    assert(slab_stats.mem_mallocd <= slab_stats.mem_limit);

    return 1;
err:
    deinit_cache_manager();
    return 0;
}

void *scmalloc(size_t size)
{
    unsigned int largest_chunk_size;
    int ffindex;
    cache_t *ccache;
    void *result;
    slab_ctl_t *cslab;

    assert(cm != NULL);
    assert(cm->caches != NULL);

    //size in bounds?
    largest_chunk_size = cm->caches[cm->cache_count-1].chunk_size;
    if (size > largest_chunk_size) {
        fprintf(stderr, "invalid size.(%lu)\r\n", (unsigned long)size);
        return NULL;
    }

    // find relevant cache
    ccache = size_to_cache(cm->caches, cm->cache_count, size);

    // need to allocate a slab_ctl?
    cslab = peek(&ccache->slabs_partial);
    if (cslab == NULL) {
        cslab = pop(&cm->slabs_free);
        if (cslab == NULL) {
            //fprintf(stderr, "no mem available.\r\n");
            return NULL;
        }
        push(&ccache->slabs_partial, cslab);
        cslab->cache = ccache;
    }

    // must be equal, axtra validation
    assert(cslab->cache == ccache);

    ffindex = ff_setbit(&cslab->slots);
    assert(ffindex != -1); // we take cslab from partial, so ffindex should be valid.
    clear_bit(&cslab->slots, ffindex);
    if (++cslab->nused == (ccache->chunk_count_perslab)) {
        pop_and_push(&ccache->slabs_partial, &ccache->slabs_full);
    }

    result = (char *)cm->slabs + cslab->nindex * SLAB_SIZE;
    result = (char *)result + ccache->chunk_size * ffindex;

    slab_stats.mem_used += ccache->chunk_size;
    
    assert(slab_stats.mem_used <= slab_stats.mem_mallocd);

    return result;
}

void scfree(void *ptr)
{
    unsigned int sidx, cidx;
    unsigned int pdiff;
    slab_ctl_t *cslab;
    int res;

    // ptr shall be in valid memory
    pdiff = (char *)ptr - (char *)cm->slabs;
    if (pdiff > (unsigned int)cm->slabctl_count*SLAB_SIZE) {
        fprintf(stderr, "invalid ptr.(%p)\r\n", ptr);
        assert(0 == 1);
        return;
    }

    sidx = pdiff / SLAB_SIZE;
    cslab = &cm->slab_ctls[sidx];
    cidx = (pdiff % SLAB_SIZE) / cslab->cache->chunk_size;

    // check if ptr is really allocated?
    if (get_bit(&cslab->slots, cidx) != 0) {
        fprintf(stderr, "ptr not allocated. double free?(%p)\r\n", ptr);
        assert(0 == 1);
        return;
    }
    set_bit(&cslab->slots, cidx);
    if (--cslab->nused == 0) {
        res = rem_and_push(&cslab->cache->slabs_partial, &cm->slabs_free, cslab);
        if (!res) {
            res = rem_and_push(&cslab->cache->slabs_full, &cm->slabs_free, cslab);
        }
        assert(res == 1); // somebody must own the slab.
    } else if (cslab->nused == cslab->cache->chunk_count_perslab-1) {
        res = rem_and_push(&cslab->cache->slabs_full, &cslab->cache->slabs_partial, cslab);
        assert(res == 1); // slabs_full must own the slab.
    } else {
        // TODO: move closer to head for efficiency?
    }

    slab_stats.mem_used -= cslab->cache->chunk_size;
}

#ifdef LC_TEST
void test_bit_set(void)
{
    bitset_t *y;
    
    y = malloc(sizeof(bitset_t));
    // for this test to work this assertion must be true.
    // change below compile-time params accordingly.
    assert(69 < WORD_SIZE_IN_BITS * WORD_COUNT);
    memset(y, 0x00, sizeof(bitset_t));

    // test ffs
    assert(get_bit(y, 69) == 0);
    set_bit(y, 69);
    assert(get_bit(y, 69) == 1);

    set_bit(y, 64);
    assert(ff_setbit(y) == 64);

    memset(y, 0x00, sizeof(bitset_t));
    assert(ff_setbit(y) == -1);
    set_bit(y, 67);
    assert(ff_setbit(y) == 67);
    assert(get_bit(y, 67) == 1);
    clear_bit(y, 67);
    assert(get_bit(y, 67) == 0);

    memset(y, 0x00, sizeof(bitset_t));
    set_bit(y, 57);
    set_bit(y, 58);
    assert(ff_setbit(y) == 57);
    set_bit(y, 56);
    assert(ff_setbit(y) == 56);
    clear_bit(y, 57);
    clear_bit(y, 58);
    clear_bit(y, 56);
    set_bit(y, 60);
    assert(ff_setbit(y) == 60);
    clear_bit(y, 60);
    assert(ff_setbit(y) == -1);

    memset(y, 0x00 ,sizeof(bitset_t));

    set_bit(y, 32); // MSB of second word
    set_bit(y, 63);
    //assert(y->words[1] == 0x80000001);
    ff_setbit(y);
    clear_bit(y, 32);
    clear_bit(y, 63);
    set_bit(y, 35);
    assert(ff_setbit(y) == 35);
    memset(y, 0x00 ,sizeof(bitset_t));
    assert(ff_setbit(y) == -1);
    set_bit(y, 96);
    set_bit(y, 97);
    set_bit(y, 104);

    assert(ff_setbit(y) == 96);
    clear_bit(y, 96);
    clear_bit(y, 97);
    assert(ff_setbit(y) == 104);

}

void test_size_to_cache(void)
{
    cache_t *cc;
    cache_t caches[9];

    caches[0].chunk_size = 2;
    caches[1].chunk_size = 4;
    cc = size_to_cache(caches, 2, 1);
    assert(cc->chunk_size == 2);
    cc = size_to_cache(caches, 2, 5);
    assert(cc == NULL);
    caches[2].chunk_size = 6;
    cc = size_to_cache(caches, 3, 3);
    assert(cc->chunk_size == 4);
    cc = size_to_cache(caches, 3, 5);
    assert(cc->chunk_size == 6);
    cc = size_to_cache(caches, 3, 6);
    assert(cc->chunk_size == 6);
    cc = size_to_cache(caches, 1, 1);
    assert(cc->chunk_size == 2);
    cc = size_to_cache(caches, 1, 3);
    assert(cc == NULL);
    cc = size_to_cache(caches, 0, 3);
    assert(cc == NULL);
    cc = size_to_cache(NULL, 1, 3);
    assert(cc == NULL);
    cc = size_to_cache(caches, INT_MAX, 3);
    assert(cc == NULL);

    caches[3].chunk_size = 8;
    caches[4].chunk_size = 10;

    cc = size_to_cache(caches, 5, 7);
    assert(cc->chunk_size == 8);
    cc = size_to_cache(caches, 5, 9);
    assert(cc->chunk_size == 10);
    cc = size_to_cache(caches, 5, 3);
    assert(cc->chunk_size == 4);
    cc = size_to_cache(caches, 5, 5);
    assert(cc->chunk_size == 6);

    caches[5].chunk_size = 12;
    caches[6].chunk_size = 14;
    caches[7].chunk_size = 16;
    caches[8].chunk_size = 18;

    cc = size_to_cache(caches, 9, 7);
    assert(cc->chunk_size == 8);
}

void test_slab_allocator(void)
{
    cache_t *cc;
    void *p;
    unsigned int i,cache_cnt;

    assert(init_cache_manager(200, 1.25) == 1);
    cache_cnt = cm->cache_count; // allocd mem will not change this value.

    // put back to free slabs
    p = scmalloc(50);
    scfree(p);

    // distribute all slabs to a single cache, and check properties
    p = scmalloc(50);
    while(scmalloc(50) != NULL) {
    }
    assert(cm->slabs_free.head == NULL); // no free slab

    cc = size_to_cache(cm->caches, cm->cache_count, 50);
    assert(cc != NULL);
    assert(cc->slabs_partial.head == NULL);
    scfree(p);
    assert(scmalloc(50) != NULL);
    assert(scmalloc(50) == NULL);

    deinit_cache_manager();
    assert(slab_stats.mem_used == 0);
    assert(slab_stats.mem_mallocd == 0);
    assert(slab_stats.mem_limit == 0);
    assert(cm == NULL);

    assert(init_cache_manager(3, 1.25) == 1);
    assert(scmalloc(cm->caches[0].chunk_size) != NULL);
    assert(scmalloc(cm->caches[1].chunk_size) != NULL);
    assert(scmalloc(cm->caches[2].chunk_size) == NULL);

    // distribute slabs uniformly
    deinit_cache_manager();
    assert(init_cache_manager(cache_cnt+1, 1.25) == 1);
    for(i=0; i < cm->cache_count; i++) {
        p = scmalloc(cm->caches[i].chunk_size);
        assert(p != NULL);
    }
    // try to fill the last slab
    for(i=1; i < (SLAB_SIZE / cm->caches[cache_cnt-1].chunk_size); i++) {
        p = scmalloc(cm->caches[cache_cnt-1].chunk_size);
        assert(p != NULL);
    }
    p = scmalloc(cm->caches[cache_cnt-1].chunk_size);
    assert(p == NULL);

    // fill a large chunk size slab
    for(i=1; i < (SLAB_SIZE / cm->caches[37].chunk_size); i++) {
        p = scmalloc(cm->caches[37].chunk_size);
        assert(p != NULL);
    }
    p = scmalloc(cm->caches[37].chunk_size);
    assert(p == NULL);

    // then fill the first slab, that has more elements
    for(i=1; i < (SLAB_SIZE / cm->caches[0].chunk_size); i++) {
        p = scmalloc(1);
        assert(p != NULL);
    }
    p = scmalloc(1);
    assert(p == NULL);
}

#endif
