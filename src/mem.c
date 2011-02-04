
#include "mem.h"

#ifdef JEMALLOC
#include "jemalloc/jemalloc.h"
#endif

void *
li_malloc(unsigned long int size)
{
    void *p;

    if (size + stats.mem_used > (settings.mem_avail)) {
        //syslog(LOG_ERR, "No memory available![%u MB]", settings.mem_avail);
        dprintf("No memory available! %lu, %u, %lu", settings.mem_avail, stats.mem_used, size);
        return NULL;
    }

    p = malloc(size+sizeof(size_t));
    memset(p, 0, size+sizeof(size_t));
    *(size_t *)p = size;
    p += sizeof(size_t);

    /* update stats */
    stats.mem_used += size;
    stats.mem_request_count++;

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