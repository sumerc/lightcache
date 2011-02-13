
#include "sys/types.h"
#include "sys/event.h"
#include "sys/time.h"

/* globals */
static int kqfd = 0;
static void (*event_handler)(conn *c, event ev) = NULL;
static struct timespec timeout;
/* constants */


/* functions */
int
event_init(void (*ev_handler)(conn *c, event ev))
{
	kqfd = kqueue();
    if (kqfd == -1) {
        syslog(LOG_ERR, "%s (%s)", "epoll create error.", strerror(errno));
        return 0;
    }
    event_handler = ev_handler;
	
	/* calculate timeout */	
	timeout.tv_sec = POLL_TIMEOUT / 1000; /* TODO: convert ms to sec/nsec */
	timeout.tv_nsec = 0
		
    return  kqfd;
}

int
event_del(conn *conn)
{
	struct kevent ke;
	
	EV_SET(&ke, conn->fd, 0, EV_DELETE, 0, 0, NULL); /* TODO: need any filter? */
    if (kevent(kqfd, &ke, 1, NULL, 0, NULL) == -1) {
		syslog(LOG_ERR, "%s (%s)", "kevent_del error.", strerror(errno));
        return 0;
	}
    return 1;
}

int
event_set(conn *c, int flags)
{
	unsigned short eflags;
	struct kevent ke;
	
	memset(&ke, 0, sizeof(struct kevent));
    
    if (flags & EVENT_READ) {
        eflags |= EVFILT_READ;
    }
    if (flags & EVENT_WRITE) {
        eflags |= EVFILT_WRITE;
    }
	
	ke.udata = c;
	EV_SET(&ke, c->fd, eflags, EV_ADD, 0, 0, NULL);
	if (kevent(kqfd, &ke, 1, NULL, 0, NULL) == -1) {
		syslog(LOG_ERR, "%s (%s)", "kevent mod. connection error.", strerror(errno));
		return 0;
	}	
    return 1;
}

void
event_process(void)
{
	int nfds;
	struct kevent events[AE_SETSIZE];
	conn *conn;
	
	nfds = kevent(kqfd, NULL, 0, events, AE_SETSIZE, &timeout);
	if (nfds == -1) {
        syslog(LOG_ERR, "%s (%s)", "kqueue wait error.", strerror(errno));
        return;
    }
	
	// process events
    for (n = 0; n < nfds; ++n) {
        conn = (struct conn *)events[n].udata;

        if ( events[n].filter == EVFILT_READ ) {
            event_handler(conn, EVENT_READ);
        }
        if (events[n].filter == EVFILT_WRITE) {
            event_handler(conn, EVENT_WRITE);
        }
    }
}




