
#ifndef SOCKET_H
#define SOCKET_H

#include "scsrv.h"
#include "fcntl.h"

#ifdef TCP

//extern int epollfd;

#define IS_READABLE(x) ( x.events & EPOLLIN)
#define IS_WRITEABLE(x) ( x.events & EPOLLOUT)
#define SOCK_TIMEOUT 5000 // in milisecs

int ychg_interest(int sock, int op, struct epoll_event ev);
int ysend(int sock, char *buf, int size);
int yrecv(int sock, char *buf, int size);
int yclose(int sock);
int ysetnonblocking(int sock);

#endif

#endif
