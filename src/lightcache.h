

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


#ifdef EPOLL
#include "sys/epoll.h"
#endif

#ifndef LIGHTCACHE_H
#define LIGHTCACHE_H

#include "protocol.h"

#define dprintf(fmt, args...) fprintf(stderr, "[+] " fmt "\n", ## args)
//#define sys_log_err(s) syslog(LOG_ERR, "%s (%s)", s, strerror(errno));

struct settings {
    unsigned int idle_conn_timeout; /* timeout in ms that idle connections will be disconnected */
    int deamon_mode; /* specify whether to run in deamon mode */
    unsigned int mem_avail; /*in bytes. max. memory this lightcache instance is allowed to use */
};

struct stats {
    size_t mem_used;
    unsigned int mem_request_count; /* number of times mem is demanded from OS */
};

typedef enum {
    READ_HEADER = 0x00,
    READ_KEY = 0x01,
    READ_DATA = 0x02,
    CONN_CLOSED = 0x03,
    CMD_RECEIVED = 0x04,
    SEND_HEADER = 0x05,
    SEND_DATA = 0x06,
    READ_EXTRA = 0x07,
} conn_states;

typedef struct conn conn;
struct conn {
    int fd; 						/* socket fd */
    int listening;					/* listening socket? */
    int active;					    /* conn have active events on the I/O interface */
    time_t last_heard; 				/* last time we heard from the client */
    conn_states state; 				/* state of the connection READ_KEY, READ_HEADER.etc...*/
    request *in;					/* request instance */
    response *out;					/* response instance */
	int free; 						/* recycle connection structure */
    conn *next;						/* next connection in the linked-list of the connections */
};

#define LIGHTCACHE_PORT 13131
#define LIGHTCACHE_LISTEN_BACKLOG 100
/*the ratio threshold that garbage collect functions will start demanding memory.
 * */
#define LIGHTCACHE_GARBAGE_COLLECT_RATIO_THRESHOLD 75
/* in sec. The timed procedures will be invoked at this interval. Like garbage collection...etc..
 * */
#define LIGHTCACHE_TIMEDRUN_INVOKE_INTERVAL 1

#endif

extern struct settings settings;
extern struct stats stats;
