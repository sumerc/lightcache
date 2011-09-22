#include "lightcache.h"

#ifndef SOCKET_H
#define SOCKET_H

#define MAX_SENDBUF_SIZE (256 * 1024 * 1024) /* same as memcached */

typedef enum {
    NEED_MORE = 0x00,
    READ_COMPLETED = 0x01,
    READ_ERR = 0x02,
    SEND_ERR = 0x03,
    SEND_COMPLETED = 0x04,
    FAILED = 0x05, // operation failed, but connection can be continued.
} socket_state;

int make_nonblocking(int sock);
int maximize_sndbuf(const int sfd);

#endif
