/*
 * cache.h
 *
 *  Created on: Aug 30, 2010
 *      Author: sumer cip
 */
#ifndef CACHE_H_
#define CACHE_H_

#include "scsrv.h"
#include "worker.h"

#define CACHE_LOCK (pthread_mutex_lock(&cache.lock))
#define CACHE_UNLOCK (pthread_mutex_unlock(&cache.lock))

typedef struct {
    char *data;
    int size;
    time_t last_access;
    int ref_count; // >0 when client is sending this, for clean cache audit.
} _img;

typedef struct {
    _htab *tab;
    pthread_mutex_t lock; // initialized at definition, see cache.c
    int req_persec;
    int sent_persec; // in KB
    unsigned long long sent_count; // in KB
    unsigned long long miss_count;
    unsigned long long req_count;
    unsigned long long mem_used; // in bytes
    int terminate;
    pthread_t thread;
} _cache;

int get_or_insert_img(char *imgname, char **res);
int create_cache_clean_audit(void);
int get_cache_statistics(char *s);
void dec_ref(_img *i);
void inc_ref(_img *i);

extern _cache cache; // extern cache defined in cache.c

#endif /* CACHE_H_ */
