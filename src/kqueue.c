
#include "sys/event.h"

// globals


// constants

// functions
int
event_init(void (*ev_handler)(conn *c, event ev))
{
    return 0;
}

int
event_del(conn *conn)
{
    return 0;
}

int
event_set(conn *c, int flags)
{
    return 0;
}

void
event_process(void)
{
    return;
}




