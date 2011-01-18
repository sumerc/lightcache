#include "log.h"

void
log_info(const char *s)
{
    syslog(LOG_INFO, "%s", s);
}

void
log_sys_err(const char *s)
{
    perror(s);
    return;
}

void 
log_err(const char *s)
{
    syslog(LOG_ERR, "%s", s);
}
