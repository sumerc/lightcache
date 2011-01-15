/*
*    An Adaptive Hash Table
*    Sumer Cip 2010
*/

#ifndef HASHTAB_H
#define HASHTAB_H

#define HSIZE(n) (1<<n)
#define HMASK(n) (HSIZE(n)-1)
#define SWAP(a, b) (((a) ^= (b)), ((b) ^= (a)), ((a) ^= (b)))
#define HLOADFACTOR 0.1

struct _hitem {
    char* key;
    int klen;
    void *val;
    int free; // for recycling.
    struct _hitem *next;
};
typedef struct _hitem _hitem;

typedef struct {
    int realsize;
    int logsize;
    int count;
    int mask;
    int freecount;
    _hitem ** _table;
} _htab;

_htab *htcreate(int logsize);
void htdestroy(_htab *ht);
_hitem *hfind(_htab *ht, char *key, int klen);
int hadd(_htab *ht, char *key, int klen, void *val);
void henum(_htab *ht, int (*fn) (_hitem *item, void *arg), void *arg, int enum_free);
int hcount(_htab *ht);
void hfree(_htab *ht, _hitem *item);

#endif
