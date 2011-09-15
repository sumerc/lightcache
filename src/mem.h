
#include "lightcache.h"

#ifndef MEM_H
#define MEM_H

void *li_malloc(size_t size);
void li_free(void *ptr);
uint64_t li_memused(void); 

#endif

