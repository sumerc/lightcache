

#ifndef FREELIST_H
#define FREELIST_H

typedef struct {
    int head;
    int size;
    int chunksize;
    void **items;
} freelist;

freelist * flcreate(int chunksize, int size);
void fldestroy(freelist *flp);
void *flget(freelist *flp);
int flput(freelist *flp, void *p);

void fldisp(freelist *flp);

#endif

