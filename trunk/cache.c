/*
 * cache.c
 *
 *  Created on: Aug 30, 2010
 *      Author: sumer cip
 */

#include "cache.h"

// public globals
_cache cache = {
    NULL,
    PTHREAD_MUTEX_INITIALIZER,
    0,
    0,
    0,
    0,
    0
};

// private globals
static time_t t0 = 0;			  // persec handling
static unsigned long long r0 = 0; // req persec
static unsigned long long s0 = 0; // out persec

void
dec_ref(_img *i)
{
    CACHE_LOCK;
    i->ref_count--;
    CACHE_UNLOCK;
}

void
inc_ref(_img *i)
{
    CACHE_LOCK;
    i->ref_count++;
    CACHE_UNLOCK;
}

static int
_add_to_cache(char *name, int namelen, _img *img)
{
    return hadd(cache.tab, name, namelen, img);
}

int
_load_file(const char *filename, _img *img)
{
    int size = 0;
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        return -1; // -1 means file opening fail
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    img->data = (char *)ymalloc(size+1);
    if (size != fread(img->data, sizeof(char), size, f)) {
        yfree(img->data);
        return -2; // -2 means file reading fail
    }
    img->size = size;
    fclose(f);
    return size;
}

static _img *
_get_from_cache(char *name, int namelen)
{
    _hitem *ret;

    ret = hfind(cache.tab, name, namelen);
    if (ret) {
        return (_img *)ret->val;
    }

    return NULL;
}

static int
_imgenumclean(_hitem *item, void * arg)
{
    _img *img;
    time_t ctime;

    ctime = time(NULL);
    img = (_img *)item->val;

    // check reference count of the item.
    if (img->ref_count > 0) {
        dprintf("ref count is not zero, cannot clean image.");
        return 1; // continue enum
    }

    //yinfo("_imgenumclean called for ctime - img->last_access:%t", ctime - img->last_access);
    if ((ctime - img->last_access) > CACHE_IMG_ITEM_CLEAN_TIME) {
        hfree(cache.tab, item);
        // we will clean the prev images from cache and this will not lead
        // to any mem leak because the will not be "free" in cache before
        // this deallocation occurs.
        yfree(img->data);
        yfree(img);

        // update cache statistics
        cache.mem_used -= img->size;

        dprintf("image cleaned.");
    }
    return 1;
}

// clean less frequently used items from the cache
// thread-safe function
void
clean_cache(void)
{
    dprintf("cache clean audit invoked.");
    CACHE_LOCK;
    henum(cache.tab, _imgenumclean, NULL, 0); // enum non-free items
    CACHE_UNLOCK;
}

// buf points to user supplied buffer, once we added buf
// to the cache, we fill this in by copying for thread-safe
// access on the data.
// thread-safe function
int
get_or_insert_img(char *imgname, char **res)
{

    int slen, nlen;
    _img *ret;
    time_t ctime;

    slen = strlen(imgname);

    CACHE_LOCK;

    ret = _get_from_cache(imgname, slen);

    if (!ret) {
        cache.miss_count++;
        ret = (_img *)ymalloc(sizeof(_img));

        nlen = _load_file(imgname, ret);
        if (nlen < 0) { // load file failed
            goto err;
        }
        if (!_add_to_cache(imgname, slen, ret)) {
            goto err;
        }
        cache.mem_used += ret->size;
    }

    // update last_access field for cache clean
    ret->last_access = time(NULL);

    // copy the cached image to the user provided buffer
    //memcpy(buf, ret->data, ret->size);
    //*len = ret->size;
    *res = (char *)ret;
    nlen = ret->size;
    ret->ref_count++;

    // update cache statistics
    cache.req_count++;
    cache.sent_count += (ret->size / 1024); // in KB
    ctime = time(NULL);
    if (ctime-t0 > 1) {
        cache.req_persec = cache.req_count - r0;
        cache.sent_persec = cache.sent_count - s0;
        r0 = cache.req_count;
        s0 = cache.sent_count;
        t0 = ctime;
    }

    CACHE_UNLOCK;

    return nlen;
err:
    syslog(LOG_ERR, "get_or_insert_img [%s] [%s]", strerror(errno),
           imgname);
    yfree(ret);
    CACHE_UNLOCK;
    return 0;
}

void *
_clean_audit_func(void *param)
{
    for (;;) {
        sleep(CACHE_CLEAN_AUDIT_INTERVAL);
        CACHE_LOCK;
        if (cache.terminate) {
            CACHE_UNLOCK;
            break;
        }
        CACHE_UNLOCK;
        clean_cache();
    }
}

int
create_cache_clean_audit(void)
{
    int rc;

    rc = pthread_create(&cache.thread, NULL, _clean_audit_func, NULL);
    return rc;
}

int
get_cache_statistics(char *s)
{
    int n;

    CACHE_LOCK;
    n = sprintf(s,
                "Cache memory:%llu, Cache requests:%llu, Cache misses:%llu, Cache free:%d, "
                "Cache request-per-sec:%d, Cache send-kb-per-sec:%d",
                cache.mem_used, cache.req_count, cache.miss_count,
                cache.tab->freecount, cache.req_persec, cache.sent_persec);
    CACHE_UNLOCK;
    return n;
}
