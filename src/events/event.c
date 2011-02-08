#include "../config.h"
#include "event.h"

#ifdef HAVE_EPOLL
#include "epoll.c"
#else
    #ifdef HAVE_KQUEUE
    #include "kqueue.c"
    #else
    #include "select.c"
    #endif
#endif

