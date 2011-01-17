#include "lightcache.h"
#include "deamonize.h"

int
main(void)
{
    // open log deamon connection
    openlog("lightcache", LOG_PID, LOG_LOCAL5);
    syslog(LOG_INFO, "lightcache started.");

    s = make_socket(LIGHTCACHE_PORT);
    make_nonblocking(s);

    listen(s, LIGHTCACHE_LISTEN_BACKLOG);
	
    epollfd = create_epoll();
    if (epollfd == -1) {
        syslog(LOG_ERR, "epoll_create [%s]", strerror(errno));
        goto err;
    }

}
