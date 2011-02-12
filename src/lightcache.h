
#include "stdio.h"
#include "syslog.h"
#include "errno.h"
#include "signal.h"
#include "string.h"
#include "stdlib.h"
#include "arpa/inet.h"
#include "time.h"
#include "assert.h"
#include "fcntl.h"
#include "unistd.h"
#include "sys/stat.h"
#include "limits.h"
#include "stdint.h"
//#include "endian.h"
#include "getopt.h"
#include "sys/socket.h"
#include "sys/types.h"
#include "netinet/in.h"

#ifndef LIGHTCACHE_H
#define LIGHTCACHE_H

struct settings {
    int deamon_mode; /* specify whether to run in deamon mode */
    uint64_t mem_avail; /*in bytes. max. memory this lightcache instance is allowed to use */
    uint64_t idle_conn_timeout; /* timeout in sec that idle connections will be disconnected */
};

struct stats {
    uint64_t mem_used;
    uint64_t mem_request_count; /* number of times mem is demanded from OS */
};

#define LIGHTCACHE_PORT 13131
#define LIGHTCACHE_LISTEN_BACKLOG 100
#define LIGHTCACHE_GARBAGE_COLLECT_RATIO_THRESHOLD 75 /*the ratio threshold that garbage collect functions will start demanding memory.*/
#define LIGHTCACHE_TIMEDRUN_INVOKE_INTERVAL 1 /* in sec. The timed procedures will be invoked at this interval. Like garbage collection...etc..*/

#endif

extern struct settings settings;
extern struct stats stats;
