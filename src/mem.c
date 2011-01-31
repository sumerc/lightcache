
#include "mem.h"
#include "jemalloc/jemalloc.h"
#include "lightcache.h"
#include "syslog.h"

void *
li_malloc(size_t size)
{
    void *p;
    
    if (size + stats.mem_used > (settings.mem_avail * 1024 * 1024 )) {
    	syslog(LOG_ERR, "No memory available![%u MB]", settings.mem_avail);
    	dprintf("No MEMORY available!");
    	return NULL;
    }

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

    if (!ptr) {
        return;
    }

    ptr = ptr - sizeof(size_t);
    size = *(size_t *)ptr;
    stats.mem_used -= size;
    free(ptr);
}