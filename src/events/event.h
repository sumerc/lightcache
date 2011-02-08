#include "../lightcache.h"
#include "../protocol.h"

#ifndef EVENT_H
#define EVENT_H

typedef enum {
    EVENT_READ = 0x01,
    EVENT_WRITE = 0x02,
} event;

int event_init(void (*ev_handler)(conn *c, event ev));
int event_set(conn *c, int flags);
int event_del(conn *c);
void event_process(void); /* call in server loop */

#endif
