
#include "stdio.h"
#include "syslog.h"
#include "errno.h"
#include "signal.h"
#include "string.h"
#include "stdlib.h"
#include "time.h"
#include "assert.h"
#include "fcntl.h"
#include "limits.h"
#include "stdint.h"
#include "getopt.h"
#include "arpa/inet.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "unistd.h"
#include "netinet/tcp.h" // TODO:not sure BSD have this.
#include "sys/un.h" // TODO:not sure BSD have this.


#ifndef __linux__
#include "sys/socket.h"
#include "netinet/in.h"
#endif

#ifndef LIGHTCACHE_H
#define LIGHTCACHE_H

// version info
#define LIGHTCACHE_VERSION 0.1
#define LIGHTCACHE_BUILD 3

struct settings {
    int deamon_mode; /* specify whether to run in deamon mode */
    uint64_t mem_avail; /*in bytes. max. memory this lightcache instance is allowed to use */
    uint64_t idle_conn_timeout; /* timeout in sec that idle connections will be disconnected */
    char *socket_path; /* path to the unix domain socket */
};

struct stats {
    uint64_t mem_used;
    uint64_t mem_request_count; /* TODO(After pre-alloc optimizations):number of times mem is demanded from OS */
    uint64_t req_per_sec;
    uint64_t resp_per_sec;
    time_t start_time;
};

#define LIGHTCACHE_PORT 13131
#define LIGHTCACHE_LISTEN_BACKLOG 100
#define LIGHTCACHE_GARBAGE_COLLECT_RATIO_THRESHOLD 75 /*the ratio threshold that garbage collect functions will start demanding memory.*/
#define LIGHTCACHE_STATS_SIZE 250

#endif

extern struct settings settings;
extern struct stats stats;

