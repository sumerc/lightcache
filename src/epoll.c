
#include "sys/epoll.h"

// globals
static int epollfd = 0;
static void (*event_handler)(conn *c, event ev) = NULL;

// constants
#define EPOLL_MAX_EVENTS 10
#define EPOLL_TIMEOUT 1000 // in ms todo: maybe get it from settings?

int
event_init(void (*ev_handler)(conn *c, event ev))
{
    epollfd = epoll_create(EPOLL_MAX_EVENTS);
    if (epollfd == -1) {
        syslog(LOG_ERR, "%s (%s)", "epoll create error.", strerror(errno));
        return 0;
    }
    event_handler = ev_handler;
    return  epollfd;
}

int
event_del(conn *conn)
{
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, conn->fd, 0) == -1) {
        syslog(LOG_ERR, "%s (%s)", "epoll_ctl disconnect conn.", strerror(errno));
        return 0;
    }
    return 1;
}

int
event_set(conn *c, int flags)
{
    int op;
    struct epoll_event ev;

    //dprintf("flags:%d, anded:%d", flags, flags & EVENT_READ);

    memset(&ev, 0, sizeof(struct epoll_event));

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
        syslog(LOG_ERR, "%s (%s)", "epoll_ctl_mod connection error.", strerror(errno));
        return 0;
    }
    return 1;
}

void
event_process(void)
{
    int nfds, n;
    struct epoll_event events[EPOLL_MAX_EVENTS];
    conn *conn;

    nfds = epoll_wait(epollfd, events, EPOLL_MAX_EVENTS, EPOLL_TIMEOUT);
    if (nfds == -1) {
        syslog(LOG_ERR, "%s (%s)", "epoll wait error.", strerror(errno));
        return;
    }

    // process events
    for (n = 0; n < nfds; ++n) {
        conn = (struct conn *)events[n].data.ptr;

        if ( events[n].events & EPOLLIN ) {
            event_handler(conn, EVENT_READ);
        }
        if (events[n].events & EPOLLOUT) {
            event_handler(conn, EVENT_WRITE);
        }

    } // process events end
}




