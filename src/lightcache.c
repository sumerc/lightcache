#include "lightcache.h"
#include "protocol.h"

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
	cli->last_heard = time(NULL);
	cli->free = 0;
	cli-> state = READ_HEADER;
	
	cli->in = malloc(sizeof(request));
	cli->out = malloc(sizeof(response));
		
	return cli;
}

int 
disconnect_client(struct client* client)
{
	// todo: cleanup request and response objects.
	
	client->free = 1;
	dprintf("disconnect client called.");
	close(client->fd);
}



void 
set_client_state(struct client* client, client_states state)
{	
	struct epoll_event ev;
	
	switch(state) {
		case READ_HEADER:			
			ev.events = EPOLLIN;
			ev.data.ptr = client;
			if (client->state != 0) {
				if (epoll_ctl(epollfd, EPOLL_CTL_MOD, client->fd, &ev) == -1) {
			        log_sys_err("epoll ctl error.");
			        //disconnect_client(client);
			        return;
			    }
			} else {
				if (epoll_ctl(epollfd, EPOLL_CTL_ADD, client->fd, &ev) == -1) {
			        log_sys_err("epoll ctl error.");
			        //disconnect_client(client);
			        return;
			    }
			}
			break;
		case READ_KEY:
			client->in->rkey = (char *)malloc(client->in->req_header.key_length + 1);		
			client->in->rkey[client->in->req_header.key_length] = (char)0;
			break;
		case READ_DATA:			
    		client->in->rdata = (char *)malloc(client->in->req_header.data_length + 1);
    		client->in->rdata[client->in->req_header.data_length] = (char)0;				
			break;
		case CMD_RECEIVED:							
			break;
		case SEND_HEADER:
			ev.events = EPOLLOUT;
			ev.data.ptr = client;
			if (epoll_ctl(epollfd, EPOLL_CTL_MOD, client->fd, &ev) == -1) {
		        log_sys_err("epoll ctl error.");
		        disconnect_client(client);
		        return;
		    } 		
		    dprintf("send_response state set for fd:%d", client->fd);              
			break;								
	}
	
	client->state = state;
	
}

void
execute_cmd(struct client* client)
{
	uint8_t cmd;
	int val;
	
	assert(client->state == CMD_RECEIVED);
	
	cmd = client->in->req_header.opcode;
	
	switch(cmd) {
		case CMD_GET:
			dprintf("CMD_GET request for key: %s:%s", client->in->rkey, client->in->rdata);
			break;
		case CMD_SET:
			dprintf("CMD_SET request for key: %s:%s", client->in->rkey, client->in->rdata);
			break;
		case CMD_CHG_SETTING:
			dprintf("CMD_CHG_SETTING request with data %s, key_length:%d", client->in->rdata, 
				client->in->req_header.key_length);
			if (!client->in->rdata) {
				break;
			}
			if (strcmp(client->in->rkey, "idle_client_timeout") == 0){	
				val = atoi(client->in->rdata);
				if (!val) {
					return; // invalid integer
				}
				settings.idle_client_timeout = val;
			}			
			break;
		case CMD_GET_SETTING:
			if (strcmp(client->in->rkey, "idle_client_timeout") == 0){
				
				client->out->sdata = malloc(sizeof(uint32_t));
				client->out->resp_header.data_length = sizeof(uint32_t);
				client->out->resp_header.opcode = client->in->req_header.opcode;
				sprintf(client->out->sdata, "%x", settings.idle_client_timeout);
				client->out->sbytes = 0;
				set_client_state(client, SEND_HEADER);
								
			}
			break;
	}
	return;
	
}

void
disconnect_idle_clients()
{
	client *client;
	
	client=clients;
	while( client != NULL && !client->free) {
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
	unsigned int needed, nbytes;
		
	needed = total - client->in->rbytes;
	
	dprintf("needed:%d", needed);
	
	nbytes = read(client->fd, &bytes[client->in->rbytes], needed);	
	if ((nbytes == 0) || (nbytes == -1)) {
		return READ_ERR;
	}	
	client->in->rbytes += nbytes;
	if (client->in->rbytes == total) {
		client->in->rbytes = 0;
		return READ_COMPLETED;
	}
	dprintf("read_nbytes returns %d", nbytes);
	return NEED_MORE;		
}

int 
try_read_request(client* client)
{
	socket_state ret;
	
	dprintf("try_read_request called %d", client->state);
	
	client->last_heard = time(NULL);
	
	switch(client->state) {		
		case READ_HEADER:	
			ret = read_nbytes(client, client->in->req_header.bytes, sizeof(req_header));
					
			if (ret == READ_COMPLETED) {
				if ( (client->in->req_header.data_length >= PROTOCOL_MAX_DATA_SIZE) || 
				   (client->in->req_header.key_length >= PROTOCOL_MAX_KEY_SIZE) ) {
					syslog(LOG_ERR, "request data or key length exceeded maximum allowed %u.", PROTOCOL_MAX_DATA_SIZE);
					return READ_ERR;
				}	
				
				// need2 read key?
				if (client->in->req_header.key_length == 0) {        			
        			set_client_state(client, CMD_RECEIVED);
        			execute_cmd(client);
        		} else {
					set_client_state(client, READ_KEY);
				}
			}						
			break;			
		case READ_KEY:
			assert(client->in->rkey != NULL);
			// todo: more asserts
			
			ret = read_nbytes(client, client->in->rkey, client->in->req_header.key_length);
			
        	if (ret == READ_COMPLETED) {	
        		
        		// need2 read data?
        		if (client->in->req_header.data_length == 0) {        			
        			set_client_state(client, CMD_RECEIVED);
        			execute_cmd(client);
        		} else {        		
					set_client_state(client, READ_DATA);
        		}
        	}		
			break;
		case READ_DATA:			
			assert(client->in->rdata != NULL);
			// todo: more asserts
			           
			ret = read_nbytes(client, client->in->rdata, client->in->req_header.data_length); 
			
        	if (ret == READ_COMPLETED) {	        		
        		set_client_state(client, CMD_RECEIVED);
        		execute_cmd(client);        		
        	}	
        	break; 
        default:
        	ret = INVALID_STATE; 
        	break;   	
	} // switch(client->state)
	
	return ret;
}

socket_state 
send_nbytes(client*client, char *bytes, size_t total)
{
	int needed, nbytes;
		
	needed = total - client->out->sbytes;
	
	nbytes = write(client->fd, &bytes[client->out->sbytes], needed);	
	if (nbytes == 0) {
		return SEND_ERR;
	}	
	client->out->sbytes += nbytes;
	if (client->out->sbytes == total) {
		client->out->sbytes = 0;
		return SEND_COMPLETED;
	}
	return NEED_MORE;		
}

int
try_send_response(client *client)
{
	socket_state ret;
	
	switch(client->state) {	
		case SEND_HEADER:
			dprintf("send header");
			ret = send_nbytes(client, client->out->resp_header.bytes, sizeof(response));
			if (ret == SEND_COMPLETED) {
				if (client->out->resp_header.data_length != 0) {
					set_client_state(client, SEND_DATA);
				}
			}
			break;
		case SEND_DATA:
			dprintf("send data %s", client->out->sdata);
			ret = send_nbytes(client, client->out->sdata, client->out->resp_header.data_length);
			if (ret == SEND_COMPLETED) {
				if (client->out->resp_header.data_length != 0) {
					set_client_state(client, READ_HEADER);// wait for new commands
				}
			}
			break;
		default:
			ret = INVALID_STATE; 
			break;
	}
	
	return ret;
	
}

int
main(void)
{	
    int s, nfds, n, optval, conn_sock;    
    struct epoll_event ev, events[LIGHTCACHE_EPOLL_MAX_EVENTS];
    struct client *client;
    int slen, ret; 
    struct sockaddr_in si_me, si_other;    
    socket_state sock_state;
    
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
	            client = make_client(conn_sock);	        
		        set_client_state(client, READ_HEADER);
	        } else {
				
				client = (struct client *)events[n].data.ptr;
				
			    if ( events[n].events & EPOLLIN ) {	
			    	sock_state = try_read_request(client);
			    	if (sock_state == READ_ERR || sock_state == INVALID_STATE) {
			    		disconnect_client(client);
	            		continue; // do not check for send events for this client any more.
			    	}
			    	
		        } 		
		        if (events[n].events & EPOLLOUT) {
		            sock_state = try_send_response(client);
		            if (sock_state == SEND_ERR || sock_state == INVALID_STATE) {
			    		disconnect_client(client);
	            		continue; // do not check for send events for this client any more.
			    	}		            
		        }
		    } // incoming connection 		    
        } // process events end     
    }  // server loop end
	
   
err:
    syslog(LOG_INFO, "lightcache stopped.");
    closelog();
    exit(EXIT_FAILURE);
}
