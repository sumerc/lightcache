
#include "mem.h"
#include "jemalloc/jemalloc.h"
#include "lightcache.h"

struct settings *settings_p = NULL;
struct stats *stats_p = NULL;

void 
init_mem(struct settings *settings, struct stats* stats)
{
	dprintf("init mem called");
	settings_p = settings;
	stats_p = stats;
	return;
}

void *
li_malloc(size_t size)
{
	return NULL;
}

void 
li_free(void *ptr)
{
	return;
}