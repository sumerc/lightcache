
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
            syslog(LOG_ERR, "No memory available![%llu MB]", (long long unsigned int)settings.mem_avail);
            LC_DEBUG(("No memory available! %llu, %llu, %u", (long long unsigned int)settings.mem_avail,
                      (long long unsigned int)mem_used, (unsigned int)size));
            return NULL;
        }

        p = malloc(size+sizeof(size_t));
        memset(p, 0, size+sizeof(size_t));
        *(size_t *)p = size;
        p = (char *)p + sizeof(size_t);

        mem_used += size;
    }
    
    return p;
}

void li_free(void *ptr)
{
    size_t size;

    if (!ptr) {
        return;
    }
    
    if (!settings.use_sys_malloc) {
        scfree(ptr);
    } else {
        ptr = (char *)ptr - sizeof(size_t);
        size = *(size_t *)ptr;
        mem_used -= size;
        free(ptr);
    }
}
