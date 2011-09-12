/*

    A General purpose Slab Allocator

      - Size ranges can be changed at compile time.
      - Slab re-assignment is possible.
      - Every bit is pre-allocated as continuous buffers. 
        Assuming good CPU cache locality.

    Sumer Cip 2011

*/

#ifndef SLAB_H
#define SLAB_H

#include "stddef.h"
#include "stdint.h"

typedef struct slab_stats_t {
    uint64_t mem_used;
    uint64_t mem_mallocd;
    uint64_t mem_limit;
    uint64_t mem_unused; // todo.
    unsigned int cache_count;
    unsigned int slab_count;
} slab_stats_t;

extern slab_stats_t slab_stats;

int init_cache_manager(size_t memory_limit, double chunk_size_factor);
void *scmalloc(size_t size);
void scfree(void *ptr);

#ifdef TEST
void test_slab_allocator(void);
void test_size_to_cache(void);
void test_bit_set(void);
#endif

#endif
