

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

#ifdef EPOLL
#include "sys/epoll.h"
#endif

#ifndef LIGHTCACHE_H
#define LIGHTCACHE_H

#define dprintf(fmt, args...) fprintf(stderr, "[&] [dbg] " fmt "\n", ## args)

typedef union {
	struct {
		uint8_t opcode;
		uint8_t key_length;
		uint32_t data_length;
	};
	uint8_t bytes[6];
}req_header;

enum client_states {
    READ_HEADER,  
    READ_DATA,
    CONN_CLOSE,
    CMD_RECEIVED,
};

struct client {
	int fd; /* socket fd */
	time_t last_heard;
	
	// protocol handling data
	req_header req_header; /* header data of the binary protocol */
	enum client_states state;
	
	// receive window
	unsigned int rbytes; /*current index into the receiving buffer, can be either for header or data.*/
	char *rbuf; /* recv buffer for data */
	
};

#define LIGHTCACHE_PORT 13131

#define LIGHTCACHE_EPOLL_MAX_EVENTS 10
#define LIGHTCACHE_LISTEN_BACKLOG 100	// 100 clients can be queued between subsequent accept()
#define EPOLL_TIMEOUT 1000 // in ms

#define IDLE_TIMEOUT 1 // in secs -- same as memcached
#define RECV_BUF_SIZE 2048 // same as memcached

#endif
