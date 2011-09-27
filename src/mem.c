
#include "mem.h"
#include "util.h"
#include "slab.h"

static uint64_t mem_used = 0;

uint64_t li_memused(void)
{
    if (!settings.use_sys_malloc) {
        return slab_stats.mem_used;
    } else {
        return mem_used;
    }
}

void *li_malloc(size_t size)
{
    void *p;

    if (!size) {
        return NULL;
    }
    
    if (!settings.use_sys_malloc) {
        p = scmalloc(size);
    } else {
        if (size + mem_used > (settings.mem_avail)) {
            syslog(LOG_ERR, "No memory available![%llu MB]\r\n", (long long unsigned int)settings.mem_avail);
            LC_DEBUG(("No memory available! [%llu, %llu, %u]\r\n", (long long unsigned int)settings.mem_avail,
                      (long long unsigned int)mem_used, (unsigned int)size));
            return NULL;
        }

        p = malloc(size+sizeof(size_t));
        memset(p, 0, size+sizeof(size_t));
        *(size_t *)p = size;
        p = (char *)p + sizeof(size_t);

        mem_used += size;
    }
    
#ifdef MEM_DEBUG
    LC_DEBUG(("Allocated memory.[%p]\r\n", p));
#endif

    return p;
}

void li_free(void *ptr)
{
    size_t size;

    if (!ptr) {
        return;
    }

#ifdef MEM_DEBUG
    LC_DEBUG(("Freeing memory.[%p]\r\n", ptr));
#endif
    
    if (!settings.use_sys_malloc) {
        scfree(ptr);
    } else {
        ptr = (char *)ptr - sizeof(size_t);
        size = *(size_t *)ptr;
        mem_used -= size;
        free(ptr);
    }
}
