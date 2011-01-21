#include "lightcache.h"

struct client* 
make_client(int fd)
{
	struct client* cli;
	
	cli = (struct client*)malloc(sizeof(struct client));
	cli->fd = fd;
	cli->state = READ_HEADER;
	cli->needbytes = sizeof(cli->req_header);
	return cli;
}

int
main(void)
{	
#ifdef TCP
    int s, nfds, n, optval, epollfd, conn_sock;    
    struct epoll_event ev, events[LIGHTCACHE_EPOLL_MAX_EVENTS];
    struct client *client;
#endif
    int slen, nbytes; 
    struct sockaddr_in si_me, si_other;
    
    slen=sizeof(si_other);

    openlog("lightcache", LOG_PID, LOG_LOCAL5);
    log_info("lightcache started.");
    

#ifdef TCP    
    if ((s=socket(AF_INET, SOCK_STREAM, 0))==-1) {
        log_sys_err("socket make error.");
        goto err;
    }
    optval = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
#endif

    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(LIGHTCACHE_PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me))==-1) {
        log_sys_err("socket bind error.");
        goto err;
    }

#ifdef TCP
    make_nonblocking(s);
    listen(s, LIGHTCACHE_LISTEN_BACKLOG);
    
    epollfd = epoll_create(LIGHTCACHE_EPOLL_MAX_EVENTS);
    if (epollfd == -1) {
        log_sys_err("epoll create error.");
        goto err;
    }

    ev.events = EPOLLIN;
    ev.data.fd = s;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, s, &ev) == -1) {
        log_sys_err("epoll ctl error.");
        goto err;
    }

#endif
    
    // initialize cache system hash table
    //cache = htcreate(IMG_CACHE_LOGSIZE);
    
    // server loop
    for (;;) {

#ifdef TCP
        nfds = epoll_wait(epollfd, events, LIGHTCACHE_EPOLL_MAX_EVENTS, EPOLL_TIMEOUT);
        if (nfds == -1) {
            log_sys_err("epoll wait error.");
            continue;
        }
        
        // process events
        for (n = 0; n < nfds; ++n) {
        	            
		    // incoming connection?
		    if (events[n].data.fd == s) {
	            conn_sock = accept(s, (struct sockaddr *)&si_other, &slen);
	            if (conn_sock == -1) {
	                log_sys_err("socket accept error.");
	                continue;
	            }
	
	            make_nonblocking(conn_sock);
	            
	            dprintf("incoming connection");
	
	            ev.events = EPOLLIN;
	            ev.data.ptr = make_client(conn_sock);		        
		        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
			    	log_sys_err("epoll ctl error.");
			    	continue;
		        }
	        } else {
				
				client = (struct client *)events[n].data.ptr;
				
			    if ( events[n].events & EPOLLIN ) {		
			    	switch (client->state) {
			            case READ_HEADER:
			            	nbytes = read(client->fd, &client->req_header, sizeof(client->req_header));
			            	client->needbytes -= nbytes;
			            	dprintf("read header total %d bytes", ( sizeof(client->req_header)-client->needbytes) );
			            	if (client->needbytes == 0) {
			            		client->state = READ_DATA;
			            	}
			            	break;
			            case READ_DATA:
			            	assert(client->needbytes != 0);
			            	
			            	//client->re			            	
			            			            	
			            	close(client->fd);
			            	break;
			    	} // switch client states 
		        } 
		
		        if (events[n].events & EPOLLOUT) {
		            dprintf("out data");
		        }
		    } // incoming connection 
		    
        } // process events end     
#endif   
    }  // server loop end
	
   
err:
    closelog();
    exit(EXIT_FAILURE);
}
