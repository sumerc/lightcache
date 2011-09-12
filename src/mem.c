
#include "mem.h"
#include "util.h"
#include "slab.h"

void *li_malloc(size_t size)
{
    void *p;
    
    // TODO: move this stats to inter mem.h thi shall be internal as we use
    // internally in slab allocator. All allocation strategies must hold their
    // own stats.
    
    if (!settings.use_sys_malloc) {
        return scmalloc(size);
    } else {
        if (size + stats.mem_used > (settings.mem_avail)) {
            syslog(LOG_ERR, "No memory available![%llu MB]", (long long unsigned int)settings.mem_avail);
            LC_DEBUG(("No memory available! %llu, %llu, %u", (long long unsigned int)settings.mem_avail,
                      (long long unsigned int)stats.mem_used, (unsigned int)size));
            return NULL;
        }

        p = malloc(size+sizeof(size_t));
        memset(p, 0, size+sizeof(size_t));
        *(size_t *)p = size;
        p = (char *)p + sizeof(size_t); /*suppress WARNING: pointer of type ‘void *’ used in arithmetic*/

        /* update stats */
        stats.mem_used += size;
        stats.mem_request_count++;

        return p;
    }
}

void li_free(void *ptr)
{
    size_t size;

    if (!ptr) {
        return;
    }

    scfree(ptr);
    return;

    

    ptr = (char *)ptr - sizeof(size_t); /*suppress WARNING: pointer of type ‘void *’ used in arithmetic */
    size = *(size_t *)ptr;
    stats.mem_used -= size;
    free(ptr);
}
