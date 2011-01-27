
#include "../lightcache.h"

#ifndef EVENT_H
#define EVENT_H

typedef enum {
    EVENT_READ = 0x01,  
    EVENT_WRITE = 0x02,
}event;


int event_init(void); 
int event_set(conn *c, int flags);
int event_del(conn *c);

#endif