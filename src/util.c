
#include "util.h"

static uint64_t
li_swap64(uint64_t in)
{
#ifdef LITTLE_ENDIAN
    /* Little endian, flip the bytes around until someone makes a faster/better
    * way to do this. */
    int64_t rv = 0;
    int i = 0;
    for(i = 0; i<8; i++) {
        rv = (rv << 8) | (in & 0xff);
        in >>= 8;
    }
    return rv;
#else
    /* big-endian machines don't need byte swapping */
    return in;
#endif
}

uint64_t ntohll(uint64_t val)
{
    return li_swap64(val);
}

uint64_t htonll(uint64_t val)
{
    return li_swap64(val);
}