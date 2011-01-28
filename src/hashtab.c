/*
*    Hash Table
*    Sumer Cip 2010
*/
#include "hashtab.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

// one_at_a_time hashing
// This is similar to the rotating hash, but it actually mixes the internal state. It takes
// 9n+9 instructions and produces a full 4-byte result. Preliminary analysis suggests there are
// no funnels. This hash was not in the original Dr. Dobb's article. I implemented it to fill
// a set of requirements posed by Colin Plumb. Colin ended up using an even simpler (and weaker)
// hash that was sufficient for his purpose.
static int
HHASH(_htab *ht, char *key, int len)
{
    int   hash, i;
    for (hash=0, i=0; i<len; ++i) {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return (hash & ht->mask);
}

static int
_hgrow(_htab *ht)
{
    int i;
    _htab *dummy;
    _hitem *p, *next, *it;

    dprintf("hgrow called growing to:%d", ht->logsize+1);

    dummy = htcreate(ht->logsize+1);
    if (!dummy)
        return 0;
    for(i=0; i<ht->realsize; i++) {
        p = ht->_table[i];
        while(p) {
            next = p->next;
            if (!hadd(dummy, p->key, p->klen, p->val))
                return 0;
            it = hfind(dummy, p->key, p->klen);
            if (!it)
                return 0;

            it->free = p->free;
            free(p->key);
            free(p);
            p = next;
        }
    }

    free(ht->_table);
    ht->_table = dummy->_table;
    ht->logsize = dummy->logsize;
    ht->realsize = dummy->realsize;
    ht->mask = dummy->mask;
    free(dummy);
    return 1;
}

_htab *
htcreate(int logsize)
{
    int i;
    _htab *ht;

    ht = (_htab *)malloc(sizeof(_htab));
    if (!ht)
        return NULL;
    ht->logsize = logsize;
    ht->realsize = HSIZE(logsize);
    ht->mask = HMASK(logsize);
    ht->count = 0;
    ht->freecount = 0;
    ht->_table = (_hitem **)malloc(ht->realsize * sizeof(_hitem *));
    if (!ht->_table) {
        free(ht);
        return NULL;
    }

    for(i=0; i<ht->realsize; i++)
        ht->_table[i] = NULL;

    return ht;
}

// val should be freed with henum(...), because obviously we cannot know if it
// contains other pointers, too.
void
htdestroy(_htab *ht)
{
    int i;
    _hitem *p, *next;

    for(i=0; i<ht->realsize; i++) {
        p = ht->_table[i];
        while(p) {
            next = p->next;
            free(p->key); // we also create keys.
            free(p);
            p = next;
        }
    }

    free(ht->_table);
    free(ht);
}


int
hadd(_htab *ht, char* key, int klen, void *val)
{
    unsigned int h;
    _hitem *new, *p;

    h = HHASH(ht, key, klen);
    p = ht->_table[h];
    new = NULL;
    while(p) {
        if ( (strcmp(p->key, key)==0) && (!p->free)) {
        	return 0;
        }
        if (p->free)
            new = p;
        p = p->next;
    }
    // have a free slot?
    if (new) {
        // do we need new allocation for the key?
        if (new->klen >= klen+1) {
            free(new->key); // free previous
            new->key = (char*)malloc(klen+1);
        }
        strncpy(new->key, key, klen+1); // copy the last "0" byte
        new->klen = klen;
        new->val = val;
        new->free = 0;
        ht->freecount--;
    } else {
        new = (_hitem *)malloc(sizeof(_hitem));
        if (!new)
            return 0;
        new->key = (char*)malloc(klen+1);
        strncpy(new->key, key, klen+1);
        new->klen = klen;
        new->val = val;
        new->next = ht->_table[h]; // add to front
        new->free = 0;
        ht->_table[h] = new;
        ht->count++;
    }
    // need resizing?
    if (((ht->count - ht->freecount) / (double)ht->realsize) >= HLOADFACTOR) {
        if (!_hgrow(ht))
            return 0;
    }
    return 1;
}

_hitem *
hfind(_htab *ht, char *key, int klen)
{
    _hitem *p;

    p = ht->_table[HHASH(ht, key, klen)];
    while(p) {
        if ((strcmp(p->key, key)==0) && (!p->free)) {
            return p;
        }
        p = p->next;
    }
    return NULL;
}

// enums non-free items
void
henum(_htab *ht, int (*enumfn)(_hitem *item, void *arg), void *arg, int enum_free)
{
    int rc, i;
    _hitem *p, *next;

    for(i=0; i<ht->realsize; i++) {
        p = ht->_table[i];
        while(p) {
            next = p->next;
            if ((!p->free) || (enum_free)) {
                rc = enumfn(p, arg); // item may be freed.
                if(rc) // abort enumeration on "0"
                    return;
            }
            p = next;
        }
    }
}

int
hcount(_htab *ht)
{
    return (ht->count - ht->freecount);
}

void
hfree(_htab *ht, _hitem *item)
{
    item->free = 1;
    ht->freecount++;
}
