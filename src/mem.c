
#include "mem.h"
#include "jemalloc/jemalloc.h"
#include "lightcache.h"


void *
li_malloc(size_t size)
{
	void *p;
	
	p = malloc(size+sizeof(size_t));	
	memset(p, 0, size+sizeof(size_t));
	*(size_t *)p = size;
	p += sizeof(size_t);	
	stats.mem_used += size;	
	return p;
}

void 
li_free(void *ptr)
{
	size_t size;
	
	ptr = ptr - sizeof(size_t);
	size = *(size_t *)ptr;
	stats.mem_used -= size;	
	free(ptr);
}