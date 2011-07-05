
#include "stdio.h"
#include "syslog.h"
#include "errno.h"
#include "signal.h"
#include "string.h"
#include "stdlib.h"
#include "time.h"
#include "assert.h"
#include "fcntl.h"
#include "unistd.h"
#include "limits.h"
#include "stdint.h"
#include "getopt.h"
#include "arpa/inet.h"
#include "sys/stat.h"
#include "netinet/tcp.h" // TODO:not sure BSD have this.

#ifndef __linux__
#include "sys/socket.h"
#include "sys/types.h"
#include "netinet/in.h"
#endif

#ifndef LIGHTCACHE_H
#define LIGHTCACHE_H

struct settings {
    int deamon_mode; /* specify whether to run in deamon mode */
    uint64_t mem_avail; /*in bytes. max. memory this lightcache instance is allowed to use */
    uint64_t idle_conn_timeout; /* timeout in sec that idle connections will be disconnected */
    char *socket_path; /* path to the unix domain socket */ 
};

struct stats {
    uint64_t mem_used;
    uint64_t mem_request_count; /* number of times mem is demanded from OS */
    uint64_t req_per_sec;
    uint64_t resp_per_sec;
};

#define LIGHTCACHE_PORT 13131
#define LIGHTCACHE_LISTEN_BACKLOG 100
#define LIGHTCACHE_GARBAGE_COLLECT_RATIO_THRESHOLD 75 /*the ratio threshold that garbage collect functions will start demanding memory.*/

#endif

extern struct settings settings;
extern struct stats stats;

