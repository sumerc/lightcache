

#include "stdio.h"
#include "syslog.h"
#include "signal.h"
#include "errno.h"
#include "string.h"
#include "stdlib.h"
#include "hashtab.h"
#include "arpa/inet.h"
#include "debug.h"
#include "pthread.h"
#include "time.h"
#include "sys/epoll.h"

#ifndef LIGHTCACHE_H
#define LIGHTCACHE_H

#define LIGHTCACHE_PORT 6666

#define MAX_EVENTS 10
#define LIGHTCACHE_LISTEN_BACKLOG 100	// 100 clients can be queued between subsequent accept()
#define EPOLL_TIMEOUT 1000 // in ms
#define IDLE_CLIENT_TIMEOUT 5 // in secs
#define IDLE_CLIENT_AUDIT_INTERVAL 10 // in secs



#endif