#include "lightcache.h"
#include "protocol.h"

#ifndef EVENT_H
#define EVENT_H

#define POLL_TIMEOUT 1000 // in ms todo: maybe get it from settings?
#define POLL_MAX_EVENTS 10

typedef enum {
    EVENT_READ = 0x01,
    EVENT_WRITE = 0x02,
} event;

int event_init(void (*ev_handler)(conn *c, event ev));

/* Deletes all previous events and set the new flags.
*/
int event_set(conn *c, int flags);

/* Deletes all associated events on the fd.
*/
int event_del(conn *c);

/* Call in server loop 
*/
void event_process(void); 

#endif
