/*
 * cache.h
 *
 *  Created on: Aug 30, 2010
 *      Author: sumer cip
 */
#ifndef CACHE_H_
#define CACHE_H_

#include "lightcache.h"

typedef struct {
    char *data;
    unsigned int size;
} cache_item_t;

typedef struct {
    _htab *tab;
    pthread_mutex_t lock;
    unsigned int req_persec;
    unsigned int sent_persec; // in KB
    unsigned int sent_count; // augmented to fit in max int
    unsigned int miss_count;
    unsigned int req_count;
    unsigned int mem_used; // augmented to fit in max int
    int terminate;
    pthread_t thread;
} cache_t;

int get(char *key);
int set(char *key);
int stats(void);

#endif /* CACHE_H_ */
