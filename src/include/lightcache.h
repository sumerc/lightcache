

#include "stdio.h"
#include "syslog.h"
#include "signal.h"
#include "errno.h"
#include "string.h"
#include "stdlib.h"
#include "arpa/inet.h"
#include "pthread.h"
#include "time.h"

#ifdef TCP

#include "sys/epoll.h"
#include "fcntl.h"

#endif


#ifndef LIGHTCACHE_H
#define LIGHTCACHE_H

struct client {
		
};

#define LIGHTCACHE_PORT 13131

#ifdef TCP
#define LIGHTCACHE_EPOLL_MAX_EVENTS 10
#define LIGHTCACHE_LISTEN_BACKLOG 100	// 100 clients can be queued between subsequent accept()
#define EPOLL_TIMEOUT 1000 // in ms
#endif

#define IDLE_TIMEOUT 1 // in secs -- same as memcached
#define RECV_BUF_SIZE 2048 // same as memcached

#endif
