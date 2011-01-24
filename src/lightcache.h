

#include "stdio.h"
#include "syslog.h"
#include "signal.h"
#include "errno.h"
#include "string.h"
#include "stdlib.h"
#include "arpa/inet.h"
#include "pthread.h"
#include "time.h"
#include "assert.h"
#include "fcntl.h"

#include "jemalloc/jemalloc.h"

#ifdef EPOLL
#include "sys/epoll.h"
#endif

#ifndef LIGHTCACHE_H
#define LIGHTCACHE_H

#define dprintf(fmt, args...) fprintf(stderr, "[&] [dbg] " fmt "\n", ## args)

struct settings {
	uint32_t idle_client_timeout;
};

typedef enum {
    NEED_MORE = 0x00,
    READ_COMPLETED = 0x01,
    READ_ERR = 0x02,
}socket_state;

typedef union {
	struct {
		uint8_t opcode;
		uint8_t key_length;
		uint32_t data_length;
	};
	uint8_t bytes[6];
}req_header;

typedef enum {
    READ_HEADER = 0x00,  
    READ_KEY = 0x01,
    READ_DATA = 0x02,
    CONN_CLOSE = 0x03,
    CMD_RECEIVED = 0x04,
    SEND_DATA = 0x05,
}client_states;

typedef struct client client;
struct client {
	int fd; 						/* socket fd */
	time_t last_heard; 				/* last time we heard from the client */
	
	
	
	/* protocol handling data */
	req_header req_header; 			/* header data of the binary protocol */
	
	client_states state;
	
	/* receive window */
	unsigned int rbytes; 			/* current index into the receiving buffer, can be either for header or data. */
	char *rdata; 					/* recv buffer for data */
	char *rkey;						/* key buffer */
	
	/* send window */
	char *sdata;
	unsigned int sbytes;
	
	int free;
	client *next;
	client *prev;
};

#define LIGHTCACHE_PORT 13131

#define LIGHTCACHE_EPOLL_MAX_EVENTS 10
#define LIGHTCACHE_LISTEN_BACKLOG 100	// 100 clients can be queued between subsequent accept()
#define EPOLL_TIMEOUT 1000 // in ms

#define PROTOCOL_MAX_KEY_SIZE 250 // in bytes --
#define PROTOCOL_MAX_DATA_SIZE 1024 + PROTOCOL_MAX_KEY_SIZE // in bytes -- same as memcached


#endif
