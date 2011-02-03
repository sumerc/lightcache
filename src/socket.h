#include "lightcache.h"

#ifndef SOCKET_H
#define SOCKET_H


typedef enum {
    NEED_MORE = 0x00,
    READ_COMPLETED = 0x01,
    READ_ERR = 0x02,
    SEND_ERR = 0x03,
    SEND_COMPLETED = 0x04,
    INVALID_STATE = 0x05,
} socket_state;

int make_nonblocking(int sock);

#endif
