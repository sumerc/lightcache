#include "lightcache.h"
#include "protocol.h"
#include "deamon.h"
#include "socket.h"
#include "hashtab.h"
#include "events/event.h"

// globals
static conn *conns = NULL; /* linked list head */
static struct settings settings;
static _htab *cache;

void 
init_settings(void)
{
	settings.idle_conn_timeout = 2; // in secs -- default same as memcached
	
	//dprintf("sizeof resp header:%d", sizeof(req_header));
	
}

struct conn* 
make_conn(int fd)
{
	struct conn *cli, *item;
	
	cli = NULL;
	// search for free items first
	for(item=conns; item!=NULL ;item=item->next) {
		if (item->free) {
			cli = item;
			break;
		}
	}
	
	// no free item?
	if (cli == NULL) {
		cli = (struct conn*)malloc(sizeof(struct conn));
		cli->next = conns;
		if (!conns) {
			conns = cli;
		}	
		
			
	}	
	cli->fd = fd;
	cli->last_heard = time(NULL);
	cli->free = 0;
	cli->active = 0;
	
	
	cli->in = malloc(sizeof(request));
	cli->out = malloc(sizeof(response));
	
	return cli;
}

int 
disconnect_conn(struct conn* conn)
{
	// todo: cleanup request and response objects.
	
	event_del(conn);
	
	conn->in->rbytes = 0;
	conn->out->sbytes = 0;
	conn->free = 1;
	dprintf("disconnect conn called.");
	close(conn->fd);
	return 1;
}



void 
set_conn_state(struct conn* conn, conn_states state)
{	
	switch(state) {
		case READ_HEADER:	
			conn->in->rbytes = 0;			
			event_set(conn, EVENT_READ);		
			break;
		case READ_KEY:
			conn->in->rkey = (char *)malloc(conn->in->req_header.key_length + 1);		
			conn->in->rkey[conn->in->req_header.key_length] = (char)0;
			break;
		case READ_DATA:			
    		conn->in->rdata = (char *)malloc(conn->in->req_header.data_length + 1);
    		conn->in->rdata[conn->in->req_header.data_length] = (char)0;				
			break;
		case READ_EXTRA:
			conn->in->rextra = (char *)malloc(conn->in->req_header.extra_length + 1);
			conn->in->rextra[conn->in->req_header.extra_length] = (char)0;
			break;
		case CMD_RECEIVED:							
			break;
		case SEND_HEADER:
			conn->out->sbytes = 0;
			event_set(conn, EVENT_WRITE);
			break;	
		default:
			break;							
	}
	
	conn->state = state;
	
}

void 
make_response(conn *conn, size_t data_length)
{
	conn->out->sdata = malloc(data_length);
	memset(conn->out->sdata, 0, data_length);
	conn->out->resp_header.data_length = data_length;
	conn->out->resp_header.opcode = conn->in->req_header.opcode;
}

void
execute_cmd(struct conn* conn)
{
	uint8_t cmd;
	int val;
	
	assert(conn->state == CMD_RECEIVED);
	
	cmd = conn->in->req_header.opcode;
	
	switch(cmd) {
		case CMD_GET:
			dprintf("CMD_GET request for key: %s:%s", conn->in->rkey, conn->in->rdata);
			break;
		case CMD_SET:
			dprintf("CMD_SET request for key: %s:%s:%s", conn->in->rkey, conn->in->rdata, 
				conn->in->rextra);
			break;
		case CMD_CHG_SETTING:
			dprintf("CMD_CHG_SETTING request with data %s, key_length:%d", conn->in->rdata, 
				conn->in->req_header.key_length);
			if (!conn->in->rdata) {
				break;
			}
			if (strcmp(conn->in->rkey, "idle_conn_timeout") == 0){	
				val = atoi(conn->in->rdata);
				if (!val) {
					return; // invalid integer
				}
				settings.idle_conn_timeout = val;				
			}			
			set_conn_state(conn, READ_HEADER);
			break;
		case CMD_GET_SETTING:
			//dprintf("CMD_GET request for key: %s:%s", conn->in->rkey, conn->in->rdata);
			if (strcmp(conn->in->rkey, "idle_conn_timeout") == 0){					
				make_response(conn, sizeof(uint32_t));		
				*(uint32_t *)conn->out->sdata = settings.idle_conn_timeout;
				set_conn_state(conn, SEND_HEADER);								
			}
			break;
	}
	return;
	
}

void
disconnect_idle_conns()
{
	conn *conn;
	
	conn=conns;
	while( conn != NULL && !conn->free && !conn->listening) {
		if (time(NULL) - conn->last_heard > settings.idle_conn_timeout) {			
			dprintf("idle conn detected.");
			disconnect_conn(conn);
			// todo: move free items closer to head for faster searching for free items in make_conn
		}
		conn=conn->next;
	}
}

socket_state 
read_nbytes(conn*conn, char *bytes, size_t total)
{
	unsigned int needed, nbytes;
		
	needed = total - conn->in->rbytes;
	
	dprintf("read_nbytes called total:%d, rbytes:%d", total, conn->in->rbytes);
	
	nbytes = read(conn->fd, &bytes[conn->in->rbytes], needed);	
	
	if (nbytes == 0) {		
		log_sys_err("socket read");
		return READ_ERR;
	} else if (nbytes == -1) {
		if (errno == EWOULDBLOCK || errno == EAGAIN) {
			log_sys_err("socket read EWOUDLBLOCK, EAGAIN:)");
			return NEED_MORE;
		}
	}	
	conn->in->rbytes += nbytes;	
	if (conn->in->rbytes == total) {
		conn->in->rbytes = 0;
		return READ_COMPLETED;
	}
	
	return NEED_MORE;		
}

int 
try_read_request(conn* conn)
{
	socket_state ret;
	
	dprintf("try_read_request called with state:%d", conn->state);
	
	conn->last_heard = time(NULL);
	
	switch(conn->state) {
		case READ_HEADER:	
			
			dprintf("req_header size:%d", sizeof(req_header));
			
			ret = read_nbytes(conn, (char *)conn->in->req_header.bytes, sizeof(req_header));
					
			if (ret == READ_COMPLETED) {
				if ( (conn->in->req_header.data_length >= PROTOCOL_MAX_DATA_SIZE) || 
				   (conn->in->req_header.key_length >= PROTOCOL_MAX_KEY_SIZE) ||
				   (conn->in->req_header.extra_length >= PROTOCOL_MAX_EXTRA_SIZE) ) {
					syslog(LOG_ERR, "request data or key length exceeded maximum allowed %u.", PROTOCOL_MAX_DATA_SIZE);
					return READ_ERR;
				}	
				
				// need2 read key?
				if (conn->in->req_header.key_length == 0) {        			
        			set_conn_state(conn, CMD_RECEIVED);
        			execute_cmd(conn);
        		} else {
					set_conn_state(conn, READ_KEY);
				}
			}						
			break;			
		case READ_KEY:
			assert(conn->in->rkey != NULL);
			// todo: more asserts
			
			ret = read_nbytes(conn, conn->in->rkey, conn->in->req_header.key_length);
			
        	if (ret == READ_COMPLETED) {	
        		
        		// need2 read data?
        		if (conn->in->req_header.data_length == 0) {        			
        			set_conn_state(conn, CMD_RECEIVED);
        			execute_cmd(conn);
        		} else {        		
					set_conn_state(conn, READ_DATA);
        		}
        	}		
			break;
		case READ_DATA:			
			assert(conn->in->rdata != NULL);
			// todo: more asserts
			           
			ret = read_nbytes(conn, conn->in->rdata, conn->in->req_header.data_length); 
			
        	if (ret == READ_COMPLETED) {	  
        		if (conn->in->req_header.extra_length) { // do we have extra data?
        			set_conn_state(conn, READ_EXTRA);
        		} else {
        			set_conn_state(conn, CMD_RECEIVED);
        			execute_cmd(conn); 
        		}       		
        	}	
        	break;
        case READ_EXTRA:
        	ret = read_nbytes(conn, conn->in->rextra, conn->in->req_header.extra_length); 
        	if (ret == READ_COMPLETED) {
        		set_conn_state(conn, CMD_RECEIVED);
        		execute_cmd(conn); 
        	}
        	break; 
        default:
        	ret = INVALID_STATE; 
        	break;   	
	} // switch(conn->state)
	
	return ret;
}

socket_state 
send_nbytes(conn*conn, char *bytes, size_t total)
{
	int needed, nbytes;
		
	needed = total - conn->out->sbytes;
	
	dprintf("send_nbytes called data:%s, total:%d, sbytes:%d", &bytes[conn->out->sbytes], 
		total, conn->out->sbytes);
	
	nbytes = write(conn->fd, &bytes[conn->out->sbytes], needed);	
	
	if (nbytes == -1) {
		if (errno == EWOULDBLOCK || errno == EAGAIN) {
			return NEED_MORE;
		}
		log_sys_err("socket write");
		return SEND_ERR;
	}	
	conn->out->sbytes += nbytes;
	if (conn->out->sbytes == total) {
		conn->out->sbytes = 0;
		return SEND_COMPLETED;
	}
	return NEED_MORE;		
}

int
try_send_response(conn *conn)
{
	socket_state ret;
	
	switch(conn->state) {	
		
		case SEND_HEADER:
		
			dprintf("send header");
			ret = send_nbytes(conn, (char *)conn->out->resp_header.bytes, sizeof(resp_header));
			if (ret == SEND_COMPLETED) {
				if (conn->out->resp_header.data_length != 0) {
					set_conn_state(conn, SEND_DATA);
				}
			}
			break;
		case SEND_DATA:
		
			dprintf("send data %s, len:%d", conn->out->sdata, conn->out->resp_header.data_length);
			ret = send_nbytes(conn, conn->out->sdata, conn->out->resp_header.data_length);
			if (ret == SEND_COMPLETED) {
				if (conn->out->resp_header.data_length != 0) {					
					set_conn_state(conn, READ_HEADER);// wait for new commands
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
    int s, nfds, n, optval, conn_sock, ret;    
    struct epoll_event ev, events[10];
    struct conn *conn;
    socklen_t slen; 
    struct sockaddr_in si_me, si_other;    
    socket_state sock_state;
    
    int silbeni;
    
    slen=sizeof(si_other);

    //malloc_stats_print(NULL, NULL, NULL);
    
    init_settings();
    
    // todo : parse and set arguments to settings here.

    openlog("lightcache", LOG_PID, LOG_LOCAL5);
    syslog(LOG_INFO, "lightcache started.");
    
	if (settings.deamon_mode) {
		deamonize();
	}
     
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
    
    ret = event_init();
    if (!ret) {
        goto err;
    }
	silbeni = ret;	
    
    conn = make_conn(s);
    conn->listening = 1;  
    event_set(conn, EVENT_READ);
    
    
    // initialize cache system hash table
    cache = htcreate(4); // todo: read from cmdline args
    
    // classic server loop
    for (;;) {
    	
        nfds = epoll_wait(silbeni, events, 10, 1000);
        if (nfds == -1) {
            log_sys_err("epoll wait error.");
            continue;
        }
        
		disconnect_idle_conns();

        // process events
        for (n = 0; n < nfds; ++n) {        	            	
			
			conn = (struct conn *)events[n].data.ptr;
			if ( events[n].events & EPOLLIN ) {	
		    	
		    	if (conn->listening) { // listening socket?
		    		dprintf("incoming conn");
				    conn_sock = accept(s, (struct sockaddr *)&si_other, &slen);
		            if (conn_sock == -1) {
		                log_sys_err("socket accept error.");
		                continue;
		            }
		            make_nonblocking(conn_sock);
		            conn = make_conn(conn_sock);	        
			        set_conn_state(conn, READ_HEADER);
		    	} else {			    	
			    	sock_state = try_read_request(conn);
			    	if (sock_state == READ_ERR || sock_state == INVALID_STATE) {
			    		disconnect_conn(conn);
	            		continue; // do not check for send events for this conn any more.
			    	}
		    	}			    	
	        } 		
	        if (events[n].events & EPOLLOUT) {
	            sock_state = try_send_response(conn);
	            if (sock_state == SEND_ERR || sock_state == INVALID_STATE) {
		    		disconnect_conn(conn);
            		continue; // do not check for send events for this conn any more.
		    	}		            
	        }
		    		    
        } // process events end     
    }  // server loop end
	
   
err:
    syslog(LOG_INFO, "lightcache stopped.");
    closelog();
    exit(EXIT_FAILURE);
}
