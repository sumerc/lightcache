
#include "lightcache.h"

#ifndef UTIL_H
#define UTIL_H

#ifndef DEBUG
#define LC_DEBUG(fmt, args...) fprintf(stderr, "[+]    " fmt "\n", ## args)
#else
#define LC_DEBUG(fmt, args...) 
#endif

void deamonize(void);
uint64_t ntohll(uint64_t val);
uint64_t htonll(uint64_t val);
int atoull(const char *s, uint64_t *ret);

#endif
