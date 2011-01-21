#include "lightcache.h"
#include "protocol.h"

struct client* 
make_client(int fd)
{
	struct client* cli;
	
	cli = (struct client*)malloc(sizeof(struct client));
	cli->fd = fd;
	cli->state = READ_HEADER;
	cli->rbuf = NULL;
	cli->rbytes = 0;
	cli->last_heard = time(NULL);
	
	return cli;
}

int 
disconnect_client(struct client* client)
{
	close(client->fd);
}

int 
set_client_state(struct client* client, enum client_states state)
{	
	client->state = state;
	return 1;
}

void
dispatch_cmd(struct client* client)
{
	uint8_t cmd;
	
	assert(client->state == CMD_RECEIVED);
	
	cmd = client->req_header.opcode;
	
	switch(cmd) {
		case CMD_GET:
			dprintf("CMD_GET request for key: %s", client->rbuf);
			break;
		case CMD_SET:
			dprintf("CMD_SET request for key: %s", client->rbuf);
			break;
	}
	
	return;
}

int 
try_read_cmd(struct client* client)
{
	int nbytes;
	
	client->last_heard = time(NULL);
	
	switch(client->state) {		
		case READ_HEADER:
			nbytes = read(client->fd, &client->req_header.bytes[client->rbytes], 1);
			dprintf("%d bytes read", nbytes);
			client->rbytes += nbytes;
					            	
			if (client->rbytes == sizeof(client->req_header)) {			            		
				client->rbytes = 0;
				client->rbuf = (char *)malloc(client->req_header.data_length + 1);		
				client->rbuf[client->req_header.data_length] = (char)0;
				set_client_state(client, READ_DATA);
			}
			break;			
			
		case READ_DATA:			
			assert(client->rbuf != NULL);
			            	
			nbytes = read(client->fd, &client->rbuf[client->rbytes], 1);
        	client->rbytes += nbytes;
        	
        	dprintf("read data :%d %d", client->req_header.opcode, client->req_header.key_length);
        	
        	if (client->rbytes == client->req_header.data_length) {			    
        		assert(strlen(client->rbuf) == client->req_header.data_length);        		
        		
        		set_client_state(client, CMD_RECEIVED);
        		
        		// parse and dispatch the request cmd
        		dispatch_cmd(client);
        		
        	}	
        	break;        	
	} // switch(client->state)
	return nbytes;
}

int
main(void)
{	
    int s, nfds, n, optval, epollfd, conn_sock;    
    struct epoll_event ev, events[LIGHTCACHE_EPOLL_MAX_EVENTS];
    struct client *client;
    int slen, ret; 
    struct sockaddr_in si_me, si_other;    
    
    slen=sizeof(si_other);

    openlog("lightcache", LOG_PID, LOG_LOCAL5);
    log_info("lightcache started.");
    

 
    if ((s=socket(AF_INET, SOCK_STREAM, 0))==-1) {
        log_sys_err("socket make error.");
        goto err;
    }
    optval = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));


    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(LIGHTCACHE_PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me))==-1) {
        log_sys_err("socket bind error.");
        goto err;
    }

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
    
    // initialize cache system hash table
    //cache = htcreate(IMG_CACHE_LOGSIZE);
    
    // server loop
    for (;;) {
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
			    	
			    	ret = try_read_cmd(client);
	            	if (ret == -1) {
	            		disconnect_client(client);
	            		continue; // do not check for send events for this client any more.
	            	}			            	
			        
		        } 
		
		        if (events[n].events & EPOLLOUT) {
		            dprintf("out data");
		        }
		    } // incoming connection 		    
        } // process events end     
    }  // server loop end
	
   
err:
    closelog();
    exit(EXIT_FAILURE);
}
