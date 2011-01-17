#include "lightcache.h"


int
main(void)
{
    int s, nfds, n;

    openlog("lightcache", LOG_PID, LOG_LOCAL5);
    //log_info("lightcache started.");
    
    //s = make_socket(LIGHTCACHE_PORT);
    //make_nonblocking(s);
    //listen_socket(s, LIGHTCACHE_LISTEN_BACKLOG);
    
    if (!iopoll_create()) {
        //log_sys_err("I/O poll interface cannot be created."); //logs with errno
        goto err;
    }
    
    iopoll_event(s, EV_READ);
    
    // initialize cache system hash table
    //cache = htcreate(IMG_CACHE_LOGSIZE);
    /*
    // server loop
    for (;;) {
        
        
        nfds = iopoll_wait();
        if (nfds == -1) {
            log_sys_err("I/O poll wait failed.");
            continue;
        }
        
        // process events
        for (n = 0; n < nfds; ++n) {
            
	    if (iopoll_readable( get_event(n) )) {		
                ;
            } else if (iopoll_writable( get_event(n) )) {
                ;
            }
            
        } // process events end        
    }  // server loop end
	*/
   
err:
    closelog();
    exit(EXIT_FAILURE);
}
