#include "lightcache.h"
#include "protocol.h"
#include "deamon.h"
#include "socket.h"
#include "hashtab.h"
#include "freelist.h"
#include "events/event.h"


/* exported globals */
struct settings settings;
struct stats stats;

/* module globals */
static conn *conns = NULL; /* linked list head */
static _htab *cache = NULL;
static freelist *response_trash; /* freelist to create response object(s) from. */
static freelist *request_trash;

void 
init_freelists(void)
{
	response_trash = flcreate(sizeof(response), 1);
	request_trash = flcreate(sizeof(request), 1);
}

void
init_settings(void)
{
    settings.idle_conn_timeout = 2; // in secs -- default same as memcached
    settings.deamon_mode = 0;
    settings.mem_avail = 64; // in MB -- default same as memcached
}

void
init_stats(void)
{
    stats.mem_used = 0;
}

struct conn*
make_conn(int fd) {
    struct conn *conn, *item;

    conn = NULL;
    // search for free items first
    for(item=conns; item!=NULL ; item=item->next) {
        if (item->free) {
            conn = item;
            break;
        }
    }

    // no free item?
    if (conn == NULL) {
        conn = (struct conn*)li_malloc(sizeof(struct conn));
        if (!conn) {
        	return NULL;
        }
        conn->next = conns;
        conns = conn;
    }

    conn->fd = fd;
    conn->last_heard = time(NULL);
    conn->active = 0;
    conn->listening = 0;    
    conn->free = 0;
    conn->in = NULL;
    conn->out = NULL;
    
    return conn;
}

static void
free_request(request *req)
{
	
	if (!req) {
		return;
	}
		
	if (req->can_free) {
		dprintf("FREEING request data.[%p]", req);
		li_free(req->rkey);
		req->rkey = NULL;
		li_free(req->rdata);
		req->rdata = NULL;
		li_free(req->rextra);
		req->rextra = NULL;
		flput(request_trash, req);
	}	
}

static void
free_response(response *resp)
{	
	
	if (!resp) {
		return;
	}
	
	if (resp->can_free) {
		dprintf("FREEING response data.[%p]", resp);
		li_free(resp->sdata);
		resp->sdata = NULL;
	}
	//li_free(resp);
	flput(response_trash, resp);
}


static int
init_resources(conn *conn)
{
	/* free previous request allocations if we have any */  
	free_request(conn->in); 
	free_response(conn->out);
	
	/* get req/resp resources from associated freelists */
	conn->in = (request *)flget(request_trash);
    if (!conn->in) {
    	return 0;
    }
    conn->out = (response *)flget(response_trash);
	if (!conn->out) {		
        return 0;
    }	
    
    /* init defaults */
    memset(&conn->in->req_header, 0, sizeof(req_header));
    memset(&conn->out->resp_header, 0, sizeof(resp_header));
    conn->in->can_free = 1; 
    conn->out->can_free = 1;
    conn->in->rbytes = 0; 
    conn->out->sbytes = 0;
    
    
    return 1;
}

static int
disconnect_conn(conn* conn)
{
	dprintf("disconnect conn called.");
	
    event_del(conn);
    
	free_request(conn->in);
	free_response(conn->out);
	
    conn->free = 1;
    close(conn->fd);

    return 1;
}



void
set_conn_state(struct conn* conn, conn_states state)
{
    switch(state) {
    case READ_HEADER:
    	
    	if (!init_resources(conn)) {
    		disconnect_conn(conn);
    		return;
    	}
    	
    	conn->in->rbytes = 0;
        event_set(conn, EVENT_READ);
        break;
    case READ_KEY:
        conn->in->rkey = (char *)li_malloc(conn->in->req_header.key_length + 1);
        if (!conn->in->rkey) {
        	disconnect_conn(conn);
        	return;
        }
        conn->in->rkey[conn->in->req_header.key_length] = (char)0;
        break;
    case READ_DATA:
        conn->in->rdata = (char *)li_malloc(conn->in->req_header.data_length + 1);
        if (!conn->in->rdata) {
        	disconnect_conn(conn);
        	return;
        }
        conn->in->rdata[conn->in->req_header.data_length] = (char)0;
        break;
    case READ_EXTRA:
        conn->in->rextra = (char *)li_malloc(conn->in->req_header.extra_length + 1);
        if (!conn->in->rextra) {
        	disconnect_conn(conn);
        	return;
        }
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

static int
prepare_response(conn *conn, size_t data_length)
{
    // todo: more asserts

    conn->out->sdata = (char *)li_malloc(data_length);
    if (!conn->out->sdata) {
        disconnect_conn(conn);
        return 0;
    }    
    conn->out->resp_header.data_length = data_length;
    conn->out->resp_header.opcode = conn->in->req_header.opcode;
    
    return 1;
}

void
execute_cmd(struct conn* conn)
{
	hresult ret;
    uint8_t cmd;
    unsigned int val;
    request *cached_req;
    _hitem *tab_item;

    assert(conn->state == CMD_RECEIVED);
    
    /* here, the complete request is received from the connection */
    conn->in->received = time(NULL);

    cmd = conn->in->req_header.opcode;

    switch(cmd) {
    case CMD_GET:

        dprintf("CMD_GET request for key: %s", conn->in->rkey);
        tab_item = hget(cache, conn->in->rkey, conn->in->req_header.key_length);
        if (!tab_item) {
            dprintf("key not found:%s", conn->in->rkey);
            return;
        }
        cached_req = (request *)tab_item->val;
        /* check timeout expire */
        val = atoi(cached_req->rextra) * 1000;
        assert( val != 0);/* CMD_SET already does this validation but re-check*/
        
        if (val + cached_req->received < time(NULL)) {
            dprintf("time expired for key:%s", conn->in->rkey);
            cached_req->can_free = 0;
            free_request(cached_req);
            hfree(cache, tab_item); // recycle tab_item 
            return;
        }
        conn->out->resp_header.data_length = cached_req->req_header.data_length;
    	conn->out->resp_header.opcode = conn->in->req_header.opcode;
        conn->out->sdata = cached_req->rdata;
        conn->out->can_free = 0;
        
        dprintf("sending GET data:%u", conn->out->resp_header.data_length);
        
        set_conn_state(conn, SEND_HEADER);
        break;

    case CMD_SET:
        dprintf("CMD_SET request for key: %s:%s:%s", conn->in->rkey, conn->in->rdata,
                conn->in->rextra);
        
		/* validate params */
		if (!conn->in->rkey) {
			dprintf("invalid key param in CMD_SET");
            return;
        }
        if (!conn->in->rdata) {
        	dprintf("invalid data param in CMD_SET");
            return;
        }
        val = atoi(conn->in->rextra) * 1000; //sec2msec
        if (!val) {
            dprintf("invalid timeout param in CMD_SET");
            return;
        }

        /* add to cache */
        ret = hset(cache, conn->in->rkey, conn->in->req_header.key_length, conn->in);
        if (ret == HEXISTS) { // key exists? then force-update the data
            tab_item = hget(cache, conn->in->rkey, conn->in->req_header.key_length);
            assert(tab_item != NULL);     
            
            cached_req = (request *)tab_item->val;
            cached_req->can_free = 1;
            free_request(cached_req);
            tab_item->val = conn->in;
        }
        conn->in->can_free = 0;
        set_conn_state(conn, READ_HEADER);
        break;
    case CMD_CHG_SETTING:
        dprintf("CMD_CHG_SETTING request with data %s, key_length:%d",
                conn->in->rdata, conn->in->req_header.key_length);
        if (!conn->in->rdata) {
            break;
        }
        if (strcmp(conn->in->rkey, "idle_conn_timeout") == 0) {
            val = atoi(conn->in->rdata);
            if (!val) {
                dprintf("invalid param in CMD_CHG_SETTING");
                return; // invalid integer
            }
            settings.idle_conn_timeout = val;
        }
        set_conn_state(conn, READ_HEADER);
        break;
    case CMD_GET_SETTING:
        dprintf("CMD_GET_SETTING request for key: %s, data:%s", conn->in->rkey,
                conn->in->rdata);
        if (strcmp(conn->in->rkey, "idle_conn_timeout") == 0) {
            if (!prepare_response(conn, sizeof(unsigned int))) {
            	return;
            }
            *(unsigned int *)conn->out->sdata = settings.idle_conn_timeout;
            set_conn_state(conn, SEND_HEADER);
        }
        break;
    case CMD_GET_STATS:
        dprintf("CMD_GET_STATS request");
        if (!prepare_response(conn, 250)) {
        	return;
        }
        sprintf(conn->out->sdata, "mem_used:%u\r\n", stats.mem_used);
        set_conn_state(conn, SEND_HEADER);
        break;
    }
    return;

}

void
disconnect_idle_conns(void)
{
    conn *conn, *next;

    conn=conns;
    while( conn != NULL && !conn->free && !conn->listening) {
        next = conn->next;
        if (time(NULL) - conn->last_heard > settings.idle_conn_timeout) {
            dprintf("idle conn detected. idle timeout:%u", settings.idle_conn_timeout);
            disconnect_conn(conn);
            // todo: move free items closer to head for faster searching for free items in make_conn
        }
        conn=next;
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
        syslog(LOG_ERR, "%s (%s)", "socket read error.", strerror(errno));
        return READ_ERR;
    } else if (nbytes == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {     
        	dprintf("socket read EWOUDLBLOCK, EAGAIN.");       
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
                //syslog(LOG_ERR, "request data or key length exceeded maximum allowed %u.", PROTOCOL_MAX_DATA_SIZE);
                dprintf("request data or key length exceeded maximum allowed");
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
        assert(conn->in->rdata != NULL); /* assert rdata malloc'd */
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

    dprintf("send_nbytes called total:%d, sbytes:%d", total, conn->out->sbytes);

    nbytes = write(conn->fd, &bytes[conn->out->sbytes], needed);

    if (nbytes == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return NEED_MORE;
        }
        syslog(LOG_ERR, "%s (%s)", "socket write error.", strerror(errno));
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
        ret = send_nbytes(conn, (char *)conn->out->resp_header.bytes, sizeof(resp_header));
        if (ret == SEND_COMPLETED) {
            if (conn->out->resp_header.data_length != 0) {
                set_conn_state(conn, SEND_DATA);
            }
        }
        break;
    case SEND_DATA:
        //dprintf("send data %s, len:%d", conn->out->sdata,
        //	conn->out->resp_header.data_length);
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

void
event_handler(conn *conn, event ev)
{
    int conn_sock, slen;
    struct sockaddr_in si_other;
    socket_state sock_state;

    slen = sizeof(si_other);

    switch(ev) {
    case EVENT_READ:
        if (conn->listening) { // listening socket?
            conn_sock = accept(conn->fd, (struct sockaddr *)&si_other, &slen);
            if (conn_sock == -1) {
            	syslog(LOG_ERR, "%s (%s)", "socket accept  error.", strerror(errno)); 
                return;
            }
            make_nonblocking(conn_sock);
            conn = make_conn(conn_sock);
            if (!conn) {
            	close(conn_sock);
            	return;
            }
            set_conn_state(conn, READ_HEADER);
        } else {
            sock_state = try_read_request(conn);
            if (sock_state == READ_ERR || sock_state == INVALID_STATE) {
                disconnect_conn(conn);
                return; // do not check for send events for this conn any more.
            }
        }

        break;
    case EVENT_WRITE:
        sock_state = try_send_response(conn);
        if (sock_state == SEND_ERR || sock_state == INVALID_STATE) {
            disconnect_conn(conn);
            return; // do not check for send events for this conn any more.
        }
        break;
    }
    return;
}

int
main(void)
{
    int s, n, optval, conn_sock, ret;
    struct sockaddr_in si_me;
    struct conn *conn;
    socklen_t slen;

    //malloc_stats_print(NULL, NULL, NULL);
    /* should be initialized before any mem operation.
     * */
	init_settings(); 
    init_stats();
	
	init_freelists();
	
	dprintf("stats:%u", stats.mem_used);
	
    // todo : parse and set arguments to settings here.

    openlog("lightcache", LOG_PID, LOG_LOCAL5);
    syslog(LOG_INFO, "lightcache started.");

    if (settings.deamon_mode) {
        deamonize();
    }

    ret = event_init(event_handler);
    if (!ret) {
        goto err;
    }

    /* init listening socket */
    if ((s=socket(AF_INET, SOCK_STREAM, 0))==-1) {
    	syslog(LOG_ERR, "%s (%s)", "socket make error.", strerror(errno)); 
        goto err;
    }
    optval = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(LIGHTCACHE_PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me))==-1) {
    	syslog(LOG_ERR, "%s (%s)", "socket bind error.", strerror(errno)); 
        goto err;
    }
    make_nonblocking(s);
    listen(s, LIGHTCACHE_LISTEN_BACKLOG);
    conn = make_conn(s);
    if (!conn) {
    	goto err;
    }
    
    conn->listening = 1;
    event_set(conn, EVENT_READ);

    /* create the in-memory hash table */
    cache = htcreate(4); // todo: read from cmdline args

    for (;;) {
        event_process();
        disconnect_idle_conns();
    }


err:
    syslog(LOG_INFO, "lightcache stopped.");
    closelog();
    exit(EXIT_FAILURE);
}
