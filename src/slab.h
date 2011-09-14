/*

A General purpose Slab Allocator

  - Size ranges can be changed at compile time.
  - Slab re-assignment is possible.
  - Every bit is pre-allocated as continuous buffers. 
    So, assuming good CPU cache locality.
  - free() is an O(1) operation whereas malloc() is 
    O(N). O(N) comes from the searching of the first set bit
    in a word array where N is roughly equals to 
    SLAB_SIZE / MIN_SLAB_CHUNK_SIZE. Theoretically, the runtime 
    O(N) behavior can be reduced to O(logN) by using a heap instead
    of a plain array with an increase of some additional memory.
    However, from practical point of view, this will degrade runtime
    performance as the number of N is very small.

Sumer Cip 2011

*/

#ifndef SLAB_H
#define SLAB_H

#include "stddef.h"
#include "stdint.h"

#define SLAB_SIZE (1024*1024)
#define MIN_SLAB_CHUNK_SIZE (40)
#define CHUNK_ALIGN_BYTES (8)

typedef struct slab_stats_t {
    uint64_t mem_used;
    uint64_t mem_mallocd;
    uint64_t mem_limit;
    uint64_t mem_used_metadata;
    unsigned int cache_count;
    unsigned int slab_count;
} slab_stats_t;

extern slab_stats_t slab_stats;

int init_cache_manager(size_t memory_limit, double chunk_size_factor);
void *scmalloc(size_t size);
void scfree(void *ptr);

#ifdef LC_TEST
void test_slab_allocator(void);
void test_size_to_cache(void);
void test_bit_set(void);
#endif

#endif
