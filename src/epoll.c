#include "iopoll.h"

#ifdef EPOLL

int
iopoll_create() 
{
    return 0;
}

void
iopoll_event(int s, int ev) 
{
    return;
}

int
iopoll_wait() {
    return 0;
}

int 
iopoll_readable(int s)
{
    return 0;
}

int 
iopoll_writable(int s)
{
    return 0;
}

#endif
