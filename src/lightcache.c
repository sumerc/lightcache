#include "lightcache.h"
#include "protocol.h"

// forward declarations
void set_client_state(struct client* client, client_states state);

// globals
static client *clients = NULL; /* linked list head */
static struct settings settings;
static int epollfd;

void 
init_settings(void)
{
	settings.idle_client_timeout = 2; // in secs -- default same as memcached
}

struct client* 
make_client(int fd)
{
	struct client *cli, *item;
	
	cli = NULL;
	// search for free items first
	for(item=clients; item!=NULL ;item=item->next) {
		if (item->free) {
			cli = item;
			break;
		}
	}
	
	// no free item?
	if (cli == NULL) {
		cli = (struct client*)malloc(sizeof(struct client));
		cli->next = clients;
		if (!clients) {
			clients = cli;
		}		
	}	
	cli->fd = fd;
	cli->state = READ_HEADER;
	cli->rkey = NULL;
	cli->rdata = NULL;
	cli->rbytes = 0;
	cli->last_heard = time(NULL);
	cli->free = 0;
	
	cli->sdata = NULL;
	cli->sbytes = 0;
	
	return cli;
}

int 
disconnect_client(struct client* client)
{
	if (client->rkey) {
		free(client->rkey);
	}
	if (client->rdata) {
		free(client->rdata);
	}
	
	client->free = 1;
	dprintf("disconnect client called.");
	close(client->fd);
}

void
execute_cmd(struct client* client)
{
	uint8_t cmd;
	int val;
	
	assert(client->state == CMD_RECEIVED);
	
	cmd = client->req_header.opcode;
	
	switch(cmd) {
		case CMD_GET:
			dprintf("CMD_GET request for key: %s:%s", client->rkey, client->rdata);
			break;
		case CMD_SET:
			dprintf("CMD_SET request for key: %s:%s", client->rkey, client->rdata);
			break;
		case CMD_CHG_SETTING:
			dprintf("CMD_CHG_SETTING request with data %s, key_length:%d", client->rdata, 
				client->req_header.key_length);
			if (strcmp(client->rkey, "idle_client_timeout") == 0){	
				val = atoi(client->rdata);
				if (!val) {
					return; // invalid integer
				}
				settings.idle_client_timeout = val;
			}			
			break;
		case CMD_GET_SETTING:
			if (strcmp(client->rkey, "idle_client_timeout") == 0){
				client->sdata = malloc(sizeof(uint32_t));
				sprintf(client->sdata, "%u", settings.idle_client_timeout);
				client->sbytes = sizeof(uint32_t);
				set_client_state(client, SEND_DATA);				
			}
			break;
	}
	
	return;
}

void 
set_client_state(struct client* client, client_states state)
{	
	struct epoll_event ev;
	
	client->state = state;
	
	switch(client->state) {
		case SEND_DATA:
			ev.events = EPOLLOUT;
			ev.data.ptr = client;
			if (epoll_ctl(epollfd, EPOLL_CTL_MOD, client->fd, &ev) == -1) {
		        log_sys_err("epoll ctl error.");
		        disconnect_client(client);
		        return;
		    } 		
		    dprintf("send_data state set for fd:%d", client->fd);              
			break;
		case CMD_RECEIVED:
			execute_cmd(client);
			break;		
		case READ_KEY:
			client->rkey = (char *)malloc(client->req_header.key_length + 1);		
			client->rkey[client->req_header.key_length] = (char)0;
			break;
		case READ_DATA:			
    		client->rdata = (char *)malloc(client->req_header.data_length + 1);
    		client->rdata[client->req_header.data_length] = (char)0;				
			break;
	}
}

void
disconnect_idle_clients()
{
	client *client;
	
	client=clients;
	while( client != NULL && !client->free && !client->sbytes) {
		if (time(NULL) - client->last_heard > settings.idle_client_timeout) {			
			dprintf("idle client detected.");
			disconnect_client(client);
			// todo: move free items closer to head for faster searching for free items in make_client
		}
		client=client->next;
	}
}

socket_state 
read_nbytes(client*client, char *bytes, size_t total)
{
	int needed, nbytes;
		
	needed = total - client->rbytes;
	nbytes = read(client->fd, &bytes[client->rbytes], needed);	
	if ((nbytes == 0) || (nbytes == -1)) {
		return READ_ERR;
	}	
	client->rbytes += nbytes;
	if (client->rbytes == total) {
		client->rbytes = 0;
		return READ_COMPLETED;
	}
	return NEED_MORE;		
}

int 
try_read_request(client* client)
{
	socket_state nbytes;
	
	dprintf("try_read_request called");
		
	client->last_heard = time(NULL);
	
	switch(client->state) {		
		case READ_HEADER:			
			
			nbytes = read_nbytes(client, client->req_header.bytes, sizeof(client->req_header));	
							            	
			if (nbytes == READ_COMPLETED) {		
				
									
				if ( (client->req_header.data_length >= PROTOCOL_MAX_DATA_SIZE) || 
				   (client->req_header.key_length >= PROTOCOL_MAX_KEY_SIZE) ) {
					syslog(LOG_ERR, "request data or key length exceeded maximum allowed %u.", PROTOCOL_MAX_DATA_SIZE);
					return READ_ERR;
				}	
				
				// need2 read key?
				if (client->req_header.key_length == 0) {        			
        			set_client_state(client, CMD_RECEIVED);
        		} else {
					set_client_state(client, READ_KEY);
				}
			}						
			break;			
		case READ_KEY:
			assert(client->rkey != NULL);
			// todo: more asserts
			
			nbytes = read_nbytes(client, client->rkey, client->req_header.key_length);
			
        	if (nbytes == READ_COMPLETED) {	
        		
        		// need2 read data?
        		if (client->req_header.data_length == 0) {        			
        			set_client_state(client, CMD_RECEIVED);
        		} else {        		
					set_client_state(client, READ_DATA);
        		}
        	}		
			break;
		case READ_DATA:			
			assert(client->rdata != NULL);
			// todo: more asserts
			           
			nbytes = read_nbytes(client, client->rdata, client->req_header.data_length); 
			
        	if (nbytes == READ_COMPLETED) {	
        		
        		set_client_state(client, CMD_RECEIVED);
        		execute_cmd(client);        		
        	}	
        	break;    	
	} // switch(client->state)
	
	return nbytes;
}

int
try_send_response(client *client)
{
	dprintf("send response for fd:%d, data:%s, length:%d", client->fd, client->sdata, client->sbytes);
	
	assert(client->state == SEND_DATA);
	
	write(client->fd, client->sdata, client->sbytes);	
	
	disconnect_client(client);
	
}

int
main(void)
{	
    int s, nfds, n, optval, conn_sock;    
    struct epoll_event ev, events[LIGHTCACHE_EPOLL_MAX_EVENTS];
    struct client *client;
    int slen, ret; 
    struct sockaddr_in si_me, si_other;    
    
    slen=sizeof(si_other);

    //malloc_stats_print(NULL, NULL, NULL);
    
    init_settings();

    openlog("lightcache", LOG_PID, LOG_LOCAL5);
    syslog(LOG_INFO, "lightcache started.");
     
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
    
    // classic server loop
    for (;;) {
        nfds = epoll_wait(epollfd, events, LIGHTCACHE_EPOLL_MAX_EVENTS, EPOLL_TIMEOUT);
        if (nfds == -1) {
            log_sys_err("epoll wait error.");
            continue;
        }
        
		disconnect_idle_clients();

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
	            
	            ev.events = EPOLLIN;
	            ev.data.ptr = make_client(conn_sock);		        
		        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
			    	log_sys_err("epoll ctl error.");
			    	continue;
		        }
	        } else {
				
				client = (struct client *)events[n].data.ptr;
				
			    if ( events[n].events & EPOLLIN ) {	
			    	
			    	if (try_read_request(client) == READ_ERR) {
			    		disconnect_client(client);
	            		continue; // do not check for send events for this client any more.
			    	}
			    	
		        } 
		
		        if (events[n].events & EPOLLOUT) {
		            try_send_response(client);
		            //dprintf("out data");
		        }
		    } // incoming connection 		    
        } // process events end     
    }  // server loop end
	
   
err:
    syslog(LOG_INFO, "lightcache stopped.");
    closelog();
    exit(EXIT_FAILURE);
}
