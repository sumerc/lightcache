/*

    A General purpose Slab Allocator

      - Size ranges can be changed at compile time.
      - Slab re-assignment is possible.
      - Every bit is pre-allocated as continigous buffers. 
        Assuming good CPU cache locality.

    Sumer Cip 2011

*/

#ifndef SLAB_H
#define SLAB_H

#include "stddef.h"

int init_cache_manager(size_t memory_limit, double chunk_size_factor);
void *scmalloc(size_t size);
void scfree(void *ptr);

void test_slab_allocator(void);
void test_size_to_cache(void);
void test_bit_set(void);

#endif
