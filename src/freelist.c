#include "freelist.h"
#include "mem.h"

static int _flgrow(freelist *flp)
{
    int i, newsize;
    void **old;

    old = flp->items;
    newsize = flp->size * 2;
    flp->items = li_malloc(newsize * sizeof(void *));
    if (!flp->items)
        return 0;

    // init new list
    for(i=0; i<flp->size; i++) {
        flp->items[i] = li_malloc(flp->chunksize);
        if (!flp->items[i]) {
            li_free(flp->items);
            return 0;
        }
    }
    // copy old list
    for(i=flp->size; i<newsize; i++)
        flp->items[i] = old[i-flp->size];

    li_free(old);
    flp->head = flp->size-1;
    flp->size = newsize;
    return 1;
}

freelist *flcreate(int chunksize, int size)
{
    int i;
    freelist *flp;

    flp = (freelist *)li_malloc(sizeof(freelist));
    if (!flp)
        return NULL;
    flp->items = li_malloc(size * sizeof(void *));
    if (!flp->items) {
        li_free(flp);
        return NULL;
    }

    for (i=0; i<size; i++) {
        flp->items[i] = li_malloc(chunksize);
        if (!flp->items[i]) {
            li_free(flp->items);
            li_free(flp);
            return NULL;
        }
    }
    flp->size = size;
    flp->chunksize = chunksize;
    flp->head = size-1;
    return flp;
}

void fldestroy(freelist *flp)
{
    int i;

    for (i=0; i<flp->size; i++) {
        li_free(flp->items[i]);
    }
    li_free(flp->items);
    li_free(flp);
}

void *flget(freelist *flp)
{
    if (flp->head < 0) {
        if (!_flgrow(flp))
            return NULL;
    }
    return flp->items[flp->head--];
}

int flput(freelist *flp, void *p)
{
    if (flp->head > flp->size-2)
        return 0;

    flp->items[++flp->head] = p;
    return 1;
}
