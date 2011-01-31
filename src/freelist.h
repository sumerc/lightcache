

#ifndef FREELIST_H
#define FREELIST_H

typedef struct freelist freelist;
struct freelist {
    int head;
    int size;
    int chunksize;
    void **items;
};

freelist * flcreate(int chunksize, int size);
void fldestroy(freelist *flp);
void *flget(freelist *flp);
int flput(freelist *flp, void *p);

#endif