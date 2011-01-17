#include "lightcache.h"
#include "deamon.h"

int
main(void)
{
    openlog("lightcache", LOG_PID, LOG_LOCAL5);
    log_info("lightcache started.");
    
    s = make_socket(LIGHTCACHE_PORT);
    make_nonblocking(s);
    listen_socket(s, LIGHTCACHE_LISTEN_BACKLOG);
    
    if (!iopoll_create()) {
        log_sys_err("I/O poll interface cannot be created."); //logs with errno
        goto err;
    }
    
    iopoll_event(s, EV_READ);
    
    // initialize cache system hash table
    cache = htcreate(IMG_CACHE_LOGSIZE);
    
    // server loop
    for (;;) {
        
        
        nfds = iopoll_wait();
        if (nfds == -1) {
            log_sys_err("I/O poll wait failed.");
            continue;
        }
        
        // process events
        for (n = 0; n < nfds; ++n) {
            
            
            if (events[n].data.fd == s) { // incoming connection?
                ;
            } else if (iopoll_readable(events[n])) {
                ;
            } else if (iopoll_writable(events[n])) {
                ;
            }
            
        } // process events end        
    }  // server loop end
}
