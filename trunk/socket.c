/*
 * socket.c
 *
 *  Created on: Aug 30, 2010
 *      Author: sumer cip
 */

#include "socket.h"

#ifdef TCP

// globals
//int epollfd;


int
ysetnonblocking(int sock)
{
    int flags;

    /* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
    /* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
    if (-1 == (flags = fcntl(sock, F_GETFL, 0)))
        flags = 0;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#else
    /* Otherwise, use the old way of doing it */
    flags = 1;
    return ioctl(sock, FIOBIO, &flags);
#endif
}

int
ychg_interest(int sock, int op, struct epoll_event ev)
{

    if (epoll_ctl(epollfd, op, sock, &ev) == -1) {
        syslog(LOG_ERR, "chg_interest [%s]", strerror(errno));
        yclose(sock);
        return -1;
    }
    return 0;
}

int
ysend(int sock, char *buf, int size)
{
    int rc;

    rc = send(sock, buf, size, 0);
    if (rc == -1) {
        if ((errno = EWOULDBLOCK) || (errno = EAGAIN)) {
            return 0; // no bytes sent
        } else {
            syslog(LOG_ERR, "_send [%s]", strerror(errno));
            yclose(sock);
            return -1;
        }
    }
    return rc;
}

int
yrecv(int sock, char *buf, int size)
{
    int rc;

    rc = recv(sock, buf, size, 0);
    if (rc == 0) {
        yclose(sock);
        return -1;
    } else if (rc == -1) {
        if ((errno = EWOULDBLOCK) || (errno = EAGAIN)) {
            return 0; // no bytes read
        } else {
            syslog(LOG_ERR, "_recv [%s]", strerror(errno));
            yclose(sock);
            return -1;
        }
    }
    return rc;
}

int
yclose(int sock)
{
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, sock, 0) == -1) {
        syslog(LOG_ERR, "worker epoll_ctl_del [%s]", strerror(errno));
        return 0;
    }
    if (close(sock) ==-1) {
        syslog(LOG_ERR, "worker close [%s]", strerror(errno));
        return 0;
    }
    return 1;
}

#endif
