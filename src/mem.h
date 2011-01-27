
#include "lightcache.h"

#ifndef MEM_H
#define MEM_H

void init_mem(struct settings *settings, struct stats* stats);
void *li_malloc(size_t size);
void li_free(void *ptr);

#endif