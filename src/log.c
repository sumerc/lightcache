#include "lightcache.h"

void
log_sys_err(const char *s)
{
    perror(s);
    syslog(LOG_ERR, "%s (%s)", s, strerror(errno));
    return;
}


