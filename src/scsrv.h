

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
#ifdef TCP
#include "sys/epoll.h"
#endif

#ifndef SCSRV_H
#define SCSRV_H

#define MAXBUFLEN 150
#define RECV_CHUNK_SIZE 25
#define CLIENT_POOL_SIZE 10
#define PORT 6666
#define IMG_CACHE_LOGSIZE 12 // 4096 slots
#define CACHE_IMG_ITEM_CLEAN_TIME 60 // in secs, if image is not accessed, then clean it in this period.
#define CACHE_CLEAN_AUDIT_INTERVAL 30 // in secs

#ifdef TCP
#define MAX_EVENTS 10
#define LISTEN_BACKLOG 100	// 100 clients can be queued between subsequent accept()
#define EPOLL_TIMEOUT 1000 // in ms
#define IDLE_CLIENT_TIMEOUT 5 // in secs
#define IDLE_CLIENT_AUDIT_INTERVAL 10 // in secs

extern int epollfd;
#endif

#ifdef UDP
typedef struct {
    struct sockaddr_in saddr;
    int slen;
    char cmd[MAXBUFLEN];
} _cdata;
#endif

#ifdef TCP

typedef struct {
    time_t last_heard;
    int 	ridx;
    char 	rwnd[MAXBUFLEN];
    int 	sidx;
    int 	slen;
    char    *swnd;
    int 	do_not_cache_swnd; // indicates whether swnd is cached, if so it is not freed.
    int 	fd;
    struct 	_client *next;
} _client;
#endif

#endif
