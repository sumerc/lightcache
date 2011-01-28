/*
*    An Adaptive Hash Table
*    Sumer Cip 2010
* 
* 	 v0.2 -- fix & optimization on hset()
*    v0.3 -- demand_mem() function for hashtable
*/

#ifndef HASHTAB_H
#define HASHTAB_H

#define HSIZE(n) (1<<n)
#define HMASK(n) (HSIZE(n)-1)
#define SWAP(a, b) (((a) ^= (b)), ((b) ^= (a)), ((a) ^= (b)))
#define HLOADFACTOR 0.75

typedef enum {
    HSUCCESS = 0x01,
    HERROR = 0x02,
    HEXISTS = 0x03, // item already exists while adding
} hresult;

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
_hitem *hget(_htab *ht, char *key, int klen);
hresult hset(_htab *ht, char *key, int klen, void *val);
void henum(_htab *ht, int (*fn) (_hitem *item, void *arg), void *arg, int enum_free);
int hcount(_htab *ht);
void hfree(_htab *ht, _hitem *item);

#endif
