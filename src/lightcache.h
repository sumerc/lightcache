

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

#include "protocol.h"

#ifdef EPOLL
#include "sys/epoll.h"
#endif

#ifndef LIGHTCACHE_H
#define LIGHTCACHE_H

#define dprintf(fmt, args...) fprintf(stderr, "[+] " fmt "\n", ## args)

typedef struct cached_item cached_item;
struct cached_item {
    unsigned int length;
    time_t timeout;
    void *data;
};

struct settings {
    unsigned int idle_conn_timeout;
    int deamon_mode; /* specify whether to run in deamon mode */
};

struct stats {
    size_t mem_used;
};


typedef enum {
    READ_HEADER = 0x00,
    READ_KEY = 0x01,
    READ_DATA = 0x02,
    CONN_CLOSE = 0x03,
    CMD_RECEIVED = 0x04,
    SEND_HEADER = 0x05,
    SEND_DATA = 0x06,
    READ_EXTRA = 0x07,
} conn_states;

typedef struct conn conn;
struct conn {
    int fd; 						/* socket fd */
    int listening;					/* listening socket? */
    int active;					    /* active in the event instance? */
    time_t last_heard; 				/* last time we heard from the client */

    conn_states state;

    /* receive window */
    request *in; /* head of linked list of request objects */

    /* send window */
    response *out; /* head of linked list of response objects */

    int free;
    int free_response_data; /* indicates if it is legal to free the response data. */
    conn *next;
};

#define LIGHTCACHE_PORT 13131
#define LIGHTCACHE_LISTEN_BACKLOG 100	// 100 clients can be queued between subsequent accept()

#endif

extern struct settings settings;
extern struct stats stats;
