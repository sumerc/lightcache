

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

struct base_packet {
	size_t data_size;
	char *data;
};

struct req_packet {
	struct base_packet base_packet;
};

struct resp_packet {
	struct base_packet base_packet;
};

struct client {
	time_t last_heard;
	
	// receive window
	size_t rsize;
	char *rbuf; /* recv buffer */
	char *rcurr; /* current pointer in to the receive buffer */			
	
	// send window
	int ssize;
	char *scurr; /* current pointer in to the send buffer  */	
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
