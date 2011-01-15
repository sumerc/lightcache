#include "scsrv.h"
#include "pthread.h"
#include "worker.h"
#include "debug.h"
#include "socket.h"
#include "cache.h"

void *
_worker_func(void *param)
{
    return NULL;
}

int
create_worker(void *param)
{
    int rc;
    pthread_t thread;

    rc = pthread_create(&thread, NULL, _worker_func, param);
    return rc;
}


