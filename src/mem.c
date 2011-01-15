#include "mem.h"

static unsigned long memused=0;

#ifdef DEBUG_MEM

static dnode_t *dhead;
static unsigned int dsize;

void YMEMLEAKCHECK(void)
{
    dnode_t *v;
    unsigned int tleak;

    v = dhead;
    tleak = 0;
    while(v) {
        fprintf(stderr, "[YMEM]    Leaked block: (addr:%p) (size:%d)\n", v->ptr, v->size);
        tleak += v->size;
        v = v->next;
    }
    if (tleak == 0)
        fprintf(stderr, "[YMEM]    Application currently has no leakage.[%d]\n", dsize);
    else
        fprintf(stderr, "[YMEM]    Application currently leaking %d bytes.[%d]\n", tleak, dsize);
}
#else
void YMEMLEAKCHECK(void)
{
    ;
}
#endif


unsigned long
ymemusage(void)
{
    return memused;
}

void *
ymalloc(size_t size)
{
    void *p;
#ifdef DEBUG_MEM
    dnode_t *v;

    p = malloc(size+sizeof(size_t));
    if (!p) {
        yerr("malloc(%d) failed. No memory?", size);
        return NULL;
    }
    memused += size;
    *(size_t *)p = size;

    if (dhead)
        yinfo("_ymalloc(%d) called[%p].[old_head:%p]", size, p, dhead->ptr);
    else
        yinfo("_ymalloc(%d) called[%p].[old_head:nil]", size, p);
    v = malloc(sizeof(dnode_t));
    v->ptr = p;
    v->size = size;
    v->next = dhead;
    dhead = v;
    dsize++;
    return (char *)p+sizeof(size_t);
#else
    p = malloc(size);
    if (!p) {
        yerr("malloc(%d) failed. No memory?", size);
        return NULL;
    }
    return p;
#endif

}

void
yfree(void *p)
{
#ifdef DEBUG_MEM
    dnode_t *v;
    dnode_t *prev;

    p = (char *)p - sizeof(size_t);
    memused -= *(size_t *)p;
    v = dhead;
    prev = NULL;
    while(v) {
        if (v->ptr == p) {
            if (prev)
                prev->next = v->next;
            else
                dhead = v->next;

            yinfo("_yfree(%p) called.", p);
            free(v);
            dsize--;
            break;
        }
        prev = v;
        v = v->next;
    }
#endif
    free(p);
}
