

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

#ifdef TCP

#include "sys/epoll.h"
#include "fcntl.h"

#endif


#ifndef LIGHTCACHE_H
#define LIGHTCACHE_H

#define dprintf(fmt, args...) fprintf(stderr, "[&] [dbg] " fmt "\n", ## args)

struct req_header {
	uint8_t opcode;
	uint8_t key_length;
	uint32_t data_length;
};

struct req_packet {
	struct req_header header;
};

struct resp_packet {
	;
};

enum client_states {
    READ_HEADER,  
    READ_DATA,
};

struct client {
	int fd; /* socket fd */
	time_t last_heard;
	
	// protocol handling data
	struct req_header req_header; /* header data of the binary protocol */
	enum client_states state;
	
	// receive window
	int needbytes;
	char *rbuf; /* recv buffer */
	char *rcurr; /* current pointer in to the receive buffer */			
	
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
