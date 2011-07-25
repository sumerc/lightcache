
#include "sys/epoll.h"

/* globals */
static int epollfd = 0;
static void (*event_handler)(conn *c, event ev) = NULL;
static struct epoll_event events[POLL_MAX_EVENTS];

/* constants */

/* functions */
int
event_init(void (*ev_handler)(conn *c, event ev))
{
    epollfd = epoll_create(POLL_MAX_EVENTS);
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
    /* todo : Note, Kernel < 2.6.9 requires a non null event pointer even for
         * EPOLL_CTL_DEL. */
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
    conn *conn;
    
    
    nfds = epoll_wait(epollfd, events, POLL_MAX_EVENTS, POLL_TIMEOUT);
    if (nfds == -1) {
        syslog(LOG_ERR, "%s (%s)", "epoll wait error.", strerror(errno));
        return;
    }

    // process events
    for (n = 0; n < nfds; ++n) {
        conn = (struct conn *)events[n].data.ptr;
        assert(conn != NULL);

        if ( events[n].events & EPOLLIN ) {
            event_handler(conn, EVENT_READ);
        }
        if (events[n].events & EPOLLOUT) {
            event_handler(conn, EVENT_WRITE);
        }

    }
}




