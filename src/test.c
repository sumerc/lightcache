
#include "stdint.h"
#include "stddef.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"
#include "math.h"
#include "limits.h"

#include "stdio.h"
#include "sys/time.h"

#define SLAB_SIZE (1024*1024)
#define MIN_SLAB_CHUNK_SIZE (40)
#define CHUNK_ALIGN_BYTES (8)
#define WORD_SIZE_IN_BITS (sizeof(word_t) * CHAR_BIT)   // in bits
#define WORD_COUNT ((SLAB_SIZE / MIN_SLAB_CHUNK_SIZE / WORD_SIZE_IN_BITS)+1)

// TODO: calculate external/internal fragmentation.

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
size_t mem_mallocd = 0;
size_t mem_limit = 0;
// TODO: some kind of var. holding the internal fragmentation (unused,wasted space)

void *
malloci(size_t size)
{
    void *ptr;
    size_t real_size;

    real_size = size+sizeof(uint64_t);
    ptr = malloc(real_size);
    memset(ptr, 0x00, real_size);
    *(uint64_t *)ptr = real_size;
    mem_mallocd += real_size;
    return ptr+sizeof(uint64_t);
}

void
freei(void *ptr)
{
    mem_mallocd -= *(uint64_t *)(ptr-sizeof(uint64_t));
    free(ptr);
}

inline unsigned int
bindex(unsigned int b)
{
    return b / WORD_SIZE_IN_BITS;
}

inline unsigned int
bloffset(unsigned int b)
{
    //return WORD_SIZE_IN_BITS - (b % WORD_SIZE_IN_BITS) - 1;
    return (b % WORD_SIZE_IN_BITS);
}

void
dump_bitset(bitset_t *bts)
{
    int i,j;

    for(i=0; i<WORD_COUNT; i++) {
        for(j=0; j<sizeof(word_t); j+=sizeof(unsigned int)) {
            printf("word %d is 0x%x\r\n", i, *(unsigned int *)(&bts->words[i]+j));
        }
    }
}

void
set_bit(bitset_t *bts, unsigned int b)
{
    assert(b < WORD_COUNT*WORD_SIZE_IN_BITS);

    bts->words[bindex(b)] |= (word_t)1 << (bloffset(b));
}
void
clear_bit(bitset_t *bts, unsigned int b)
{
    assert(b < WORD_COUNT*WORD_SIZE_IN_BITS);

    bts->words[bindex(b)] &= ~((word_t)1 << (bloffset(b)));
}

unsigned int
get_bit(bitset_t *bts, unsigned int b)
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
int
nlz64(register uint64_t x)
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

static int
ff_setbit(bitset_t *bts)
{
    int i, j;
    word_t w;

    for(i=0; i<WORD_COUNT; i++) {
        w = bts->words[i];
        if(w) {
            j = nlz64(w);
            //printf("dd:%d, %d, %d\r\n", j, i, j + (i*WORD_SIZE_IN_BITS));
            return (j + (i*WORD_SIZE_IN_BITS));
        }
    }

    return -1;
}

// TODO: Need any tests for the doubly linked list?
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
        //printf("cit:%p\r\n", cit);
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

static inline double
logbn(double base, double x)
{
    return log(x) / log(base);
}

// A binary search like routine for ceiling the requested size to a cache with proper size.
// Implemented to reduce the mapping complexity to O(logn) in scmalloc()'s.
cache_t *
size_to_cache(cache_t *arr, unsigned int arr_size, unsigned int key)
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
    } else if (r < arr_size-1) {
        if (key <= arr[r+1].chunk_size) {
            return &arr[r+1];
        }
    }

    return NULL;
}

static void
deinit_cache_manager(void)
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
    mem_used = mem_limit = 0;

    assert(mem_mallocd == 0);// all real-mallocd chunks shall be freed here.
}

// TODO: Do we need to check return value of malloc here?
static int
init_cache_manager(size_t memory_limit, double chunk_size_factor)
{
    unsigned int size,i;
    slab_ctl_t *prev_slab;

    if (cm != NULL) {
        fprintf(stderr, "slab allocator is already initialized.\r\n");
        return 0;
    }

    // initialize globals
    mem_limit = memory_limit * 1024*1024; // memory_limit is in MB
    cm = malloci(sizeof(cache_manager_t));

    // cache_count is calculated by starting from the MIN_SLAB_CHUNK_SIZE and
    // multiplying it with chunk_size_factor for every iteration till we reach
    // SLAB_SIZE. This idea is being used on memcached() and proved to be well
    // on real-world.
    cm->cache_count = (unsigned int)ceil(logbn(chunk_size_factor,
                                         SLAB_SIZE/MIN_SLAB_CHUNK_SIZE))-1;

    // alloc/initialize caches
    cm->caches = malloci(sizeof(cache_t)*cm->cache_count);
    for(i=0,size=MIN_SLAB_CHUNK_SIZE; i < cm->cache_count; size*=chunk_size_factor, i++) {
        size = align_bytes(size);
        cm->caches[i].chunk_size = size;
    }

    // calculate remaining memory for slabs. sizeof(uint64_t) is the bytes
    // allocated at every malloced chunk. As we will allocate slab_ctls and
    // slabs further, just calculate them too.
    // TODO: Any chance testing bounds on below calculation?
    cm->slabctl_count = (mem_limit-mem_mallocd-(2*sizeof(uint64_t))) /
                        (SLAB_SIZE+sizeof(slab_ctl_t));
    if (cm->slabctl_count <= 1) {
        deinit_cache_manager();
        fprintf(stderr, "not enough mem to create a slab\r\n");
        return 0;
    }

    // alloc/initialize slab_ctl and slabs
    cm->slab_ctls = malloci(sizeof(slab_ctl_t)*cm->slabctl_count);
    cm->slabs = malloci(SLAB_SIZE*cm->slabctl_count);
    cm->slabs_free.head = cm->slab_ctls;
    cm->slabs_free.tail = &cm->slab_ctls[cm->slabctl_count-1];
    prev_slab = NULL;
    for(i=0; i < cm->slabctl_count; i++) {
        cm->slab_ctls[i].prev = prev_slab;
        if (i == cm->slabctl_count-1) { // last element?
            cm->slab_ctls[i].next = NULL;
        } else {
            cm->slab_ctls[i].next = &cm->slab_ctls[i+1];
        }
        cm->slab_ctls[i].nindex = i;

        // setbit indicates free slot.
        memset(&cm->slab_ctls[i].slots, 0xFF, sizeof(word_t)*WORD_COUNT);

        prev_slab = &cm->slab_ctls[i];
    }

    // mem_alloc shall always be smaller than mem_limit
    assert(mem_mallocd <= mem_limit);

    return 1;
}

void *
scmalloc(size_t size)
{
    unsigned int ffindex, largest_chunk_size;
    cache_t *ccache;
    void *result;
    slab_ctl_t *cslab;

    //size in bounds?
    largest_chunk_size = cm->caches[cm->cache_count-1].chunk_size;
    if (size > largest_chunk_size) {
        fprintf(stderr, "Invalid size.(%u)\r\n", size);
        return NULL;
    }

    // find relevant cache
    ccache = size_to_cache(cm->caches, cm->cache_count, size);

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

    ffindex = ff_setbit(&cslab->slots);
    assert(ffindex != -1); // we take cslab from partial, so ffindex should be valid.
    clear_bit(&cslab->slots, ffindex);
    if (++cslab->nused == (SLAB_SIZE / ccache->chunk_size)) {
        cslab = pop(&ccache->slabs_partial);
        push(&ccache->slabs_full, cslab);
        // TODO: assert slots is empty via caculating the ffindex is greater than
        // the alloc'd bits.
    }

    result = cm->slabs + cslab->nindex * SLAB_SIZE;
    result += ccache->chunk_size * ffindex;

    mem_used += ccache->chunk_size;

    assert(mem_used <= mem_mallocd);

    return result;
}

void
scfree(void *ptr)
{
    unsigned int sidx, cidx;
    ptrdiff_t pdiff;
    slab_ctl_t *cslab;
    int res;

    // ptr shall be in valid memory
    pdiff = ptr - cm->slabs;
    if (pdiff > cm->slabctl_count*SLAB_SIZE) {
        fprintf(stderr, "Invalid ptr.(%p)\r\n", ptr);
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
    //fprintf(stderr, "sidx:%u, cidx:%u.\r\n", sidx, cidx);
    set_bit(&cslab->slots, cidx);
    if (--cslab->nused == 0) {
        // we shall have no unfree chunk here
        // TODO: Implement assert(ff_setbit(&cslab->slots) == -1);

        if (!rem(&cslab->cache->slabs_partial, cslab)) {
            res = rem(&cslab->cache->slabs_full, cslab);
            assert(res == 1); // somebody must own the slab.
        }
        push(&cm->slabs_free, cslab);
    }  // move from full to partial
    else if (cslab->nused == (SLAB_SIZE / cslab->cache->chunk_size)-1) {
        res = rem(&cslab->cache->slabs_full, cslab);
        assert(res == 1); // slabs_full must own the slab.
        push(&cslab->cache->slabs_partial, cslab);
    } else {
        // TODO: move closer to head for efficiency?
    }

    mem_used -= cslab->cache->chunk_size;
}

// TESTS....
inline long long
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
test_bit_set(void)
{
    bitset_t *y;
    long long t0;
    t0 = tickcount();

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

    fprintf(stderr,
            "[+]    test_bit_set. (ok) (elapsed:%0.6f)\r\n", (tickcount()-t0)*0.000001);
}

void
test_size_to_cache(void)
{
    long long t0;
    cache_t *cc;

    t0 = tickcount();
    cache_t caches[9];

    caches[0].chunk_size = 2;
    caches[1].chunk_size = 4;
    cc = size_to_cache(caches, 2, 1);
    //printf("bibk:%u\r\n", cc->chunk_size);
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

    fprintf(stderr,
            "[+]    test_size_to_cache. (ok) (elapsed:%0.6f)\r\n", (tickcount()-t0)*0.000001);
}

// TODO: Test different slab states. Distribute slabs to different caches.
// Think deep:)
void
test_slab_allocator(void)
{
    long long t0;
    cache_t *cc;
    void *p;
    int i;
    unsigned int cache_cnt;

    t0 = tickcount();

    assert(init_cache_manager(200, 1.25) == 1);
    cache_cnt = cm->cache_count; // allocd mem will not change this value.

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
    assert(mem_used == 0);
    assert(mem_mallocd == 0);
    assert(mem_limit == 0);
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

    fprintf(stderr,
            "[+]    test_slab_allocator. (ok) (elapsed:%0.6f)\r\n", (tickcount()-t0)*0.000001);
}


int
main(void)
{
    test_bit_set();
    test_slab_allocator();
    test_size_to_cache();

    return 0;
}