
#include "event.h"
#include "../log.h"

// globals
static int epollfd;

// constants
#define EPOLL_MAX_EVENTS 10
#define EPOLL_TIMEOUT 1000 // in ms todo: maybe get it from settings?

int 
event_init(void)
{
	epollfd = epoll_create(EPOLL_MAX_EVENTS);
	if (epollfd == -1) {
        log_sys_err("epoll create error.");
        return 0;
    } 
    return  epollfd;    
}

int 
event_del(conn *conn)
{
	if (epoll_ctl(epollfd, EPOLL_CTL_DEL, conn->fd, 0) == -1) {
        log_sys_err("epoll_ctl disconnect conn");
        return 0;
    }
    return 1;
}

int 
event_set(conn *c, int flags)
{
	int op;
	struct epoll_event ev;
	
	ev.events = 0x0;	
	if (flags & EVENT_READ) {
		ev.events |= EPOLLIN;	
	}
	if (flags & EVENT_WRITE) {
		ev.events |= EPOLLOUT;	
	}
	
	
	if (!c->active) {
		op = EPOLL_CTL_ADD;
		c->active = 1;
	} else {
		op = EPOLL_CTL_MOD;
	}
	
	ev.data.ptr = c;	
	if (epoll_ctl(epollfd, op, c->fd, &ev) == -1) {
        log_sys_err("epoll_ctl_mod conn");
        return 0;
	}	
	return 1;
}






