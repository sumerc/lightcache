
#include "lightcache.h"

#ifndef UTIL_H
#define UTIL_H

#ifdef DEBUG
#define LC_DEBUG(x) printf x
#else
#define LC_DEBUG(x)
#endif

#define CURRENT_TIME time(NULL)

void sig_handler(int signum);
void deamonize(void);
uint64_t ntohll(uint64_t val);
uint64_t htonll(uint64_t val);
int atoull(const char *s, uint64_t *ret);


#ifdef LC_TEST
void test_endianness(void);
void test_util_routines(void);
#endif

#endif
