#include "lightcache.h"
#include "protocol.h"
#include "socket.h"
#include "hashtab.h"
#include "event.h"
#include "mem.h"
#include "util.h"
#include "slab.h"
#include "sys/resource.h"

/* forward declarations */
void set_conn_state(struct conn* conn, conn_states state);

/* exported globals */
struct settings settings;
struct stats stats;

/* module globals */
static conn *conns = NULL; /* linked list head */
static _htab *cache = NULL;

// initialize defaults for settings
void init_settings(void)
{
#ifndef DEBUG
    settings.deamon_mode = 1;
    settings.idle_conn_timeout = 2; // in secs -- default same as memcached
#else
    settings.deamon_mode = 0;
    settings.idle_conn_timeout = 4000000000; // near infinite for testing
#endif
    settings.mem_avail = 64 * 1024 * 1024; // in bytes -- 64 mb -- default same as memcached
    settings.socket_path = NULL;    // unix domain socket is off by default.
    settings.use_sys_malloc = 0;
    settings.fd_limit = 1024; // rlimit_nofile -- requires root
}

void init_log(void)
{
    openlog("lightcache", LOG_PID, LOG_LOCAL5);
    syslog(LOG_INFO, "lightcache started.");
}

void init_stats(void)
{
    stats.start_time = CURRENT_TIME;
    stats.curr_connections = 0;
    stats.cmd_get = 0;
    stats.cmd_set = 0;
    stats.get_hits = 0;
    stats.get_misses = 0;
    stats.bytes_read = 0;
    stats.bytes_written = 0;
}

struct conn* make_conn(int fd) {
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
    conn->last_heard = CURRENT_TIME;
    conn->listening = 0;
    conn->free = 0;
    conn->in = NULL;

    stats.curr_connections++;

    return conn;
}

// NOTE: htab key is re-used so we do not free it. 
static void del_cached_req(_hitem *it)
{
    request *req;
        
    req = ((request *)it->val);
    if (req) {
        li_free(req->rkey);
        li_free(req->rdata);
        li_free(req->rextra);
        li_free(req);
    }
    it->val = NULL;
}

static void free_request(conn *conn)
{
    request *req;

    req = conn->in;
    if ((!req) || (!req->can_free)) {
        return;
    }

    li_free(req->rkey);
    li_free(req->rdata);
    li_free(req->rextra);
    li_free(req);
    conn->in = NULL;
}

static void free_response(conn *conn)
{
    response *resp;
    
    resp = &conn->out;
    if (resp->can_free) {
        li_free(resp->sdata);
    }
    resp->sdata = NULL;
}


static int init_resources(conn *conn)
{         
    free_request(conn);
    free_response(conn);
             
    conn->in = (request *)li_malloc(sizeof(request));
    if (!conn->in) {
        return 0;
    }
  
    conn->in->rbytes = 0;    
    conn->in->rkey = NULL;
    conn->in->rdata = NULL;
    conn->in->rextra = NULL;    
    conn->in->can_free = 1;  

    conn->out.sdata = NULL;
    conn->out.sbytes = 0;
    conn->out.can_free = 1;
   
    return 1;
}

static void disconnect_conn(conn* conn)
{
    LC_DEBUG(("disconnect conn called.\r\n"));

    free_request(conn);
    free_response(conn);

    conn->free = 1;

    event_del(conn);
    close(conn->fd);

    stats.curr_connections--;

    set_conn_state(conn, CONN_CLOSED);
}

static void add_response(conn *conn, void *data, size_t data_length, code_t code)
{
    conn->out.resp_header.response.data_length = htonl(data_length);
    conn->out.resp_header.response.opcode = conn->in->req_header.request.opcode;
    conn->out.resp_header.response.retcode = code;
    
    conn->out.sdata = data;
    conn->out.can_free = 1;

    set_conn_state(conn, SEND_HEADER);    
} 

static void send_response(conn *conn, code_t code)
{
    add_response(conn, NULL, 0, code);
}


void set_conn_state(struct conn* conn, conn_states state)
{
    switch(state) {
    case READ_HEADER:
        if (!init_resources(conn)) {
            LC_DEBUG(("req/resp resources cannot be initialized.\r\n"));
            disconnect_conn(conn); // we cannot receive anything.
            return;
        }
        event_set(conn, EVENT_READ);
        break;
    case READ_KEY:
        conn->in->rkey = (char *)li_malloc(conn->in->req_header.request.key_length + 1);
        if (!conn->in->rkey) {
            send_response(conn, OUT_OF_MEMORY);
            return;
        }
        conn->in->rkey[conn->in->req_header.request.key_length] = (char)0;
        break;
    case READ_DATA:
        conn->in->rdata = (char *)li_malloc(conn->in->req_header.request.data_length + 1);
        if (!conn->in->rdata) {
            send_response(conn, OUT_OF_MEMORY);
            return;
        }
        conn->in->rdata[conn->in->req_header.request.data_length] = (char)0;
        break;
    case READ_EXTRA:
        conn->in->rextra = (char *)li_malloc(conn->in->req_header.request.extra_length + 1);
        if (!conn->in->rextra) {
            send_response(conn, OUT_OF_MEMORY);
            return;
        }
        conn->in->rextra[conn->in->req_header.request.extra_length] = (char)0;
        break;
    case CMD_RECEIVED:
        break;
    case CMD_SENT:
        break;
    case SEND_HEADER:
        conn->out.sbytes = 0;
        event_set(conn, EVENT_WRITE);
        break;
    default:
        break;
    }

    conn->state = state;

}

static int flush_item_enum(_hitem *item, void *arg)
{
    if (arg) {
        ;   // suppress unused param. warning.
    }
    
    LC_DEBUG(("flush_item called.\r\n"));
    
    del_cached_req(item);
    hfree(cache, item);

    return 0;
}


static void execute_cmd(struct conn* conn)
{
    int r;
    hresult ret;
    uint8_t cmd;
    uint64_t val;
    request *cached_req;
    _hitem *tab_item;
    char *sval;
    uint64_t *ival;

    assert(conn->state == CMD_RECEIVED);

    /* here, the complete request is received from the connection */
    conn->in->received = CURRENT_TIME;
    cmd = conn->in->req_header.request.opcode;
    
    /* No need for the validation of conn->in->rkey as it is mandatory for the
       protocol. */
    switch(cmd) {
    case CMD_GET:

        LC_DEBUG(("CMD_GET [%s]\r\n", conn->in->rkey));

        stats.cmd_get++;

        /* get item */
        tab_item = hget(cache, conn->in->rkey, conn->in->req_header.request.key_length);
        if (!tab_item) {
            LC_DEBUG(("Key not found:%s\r\n", conn->in->rkey));
            goto GET_KEY_NOTEXISTS;
        }
        cached_req = (request *)tab_item->val;

        /* check timeout expire */
        r = atoull(cached_req->rextra, &val);
        assert(r != 0);/* CMD_SET already does this validation but re-check*/

        if ((unsigned int)(conn->in->received-cached_req->received) > val) {
            LC_DEBUG(("Time expired for key:%s\r\n", conn->in->rkey));
            del_cached_req(tab_item);
            hfree(cache, tab_item);
            goto GET_KEY_NOTEXISTS;
        }

        stats.get_hits++;

        add_response(conn, cached_req->rdata, cached_req->req_header.request.data_length, SUCCESS);
        conn->out.can_free = 0;
        break;
    case CMD_SET:

        LC_DEBUG(("CMD_SET \r\n"));

        stats.cmd_set++;

        // validate params
        if (!conn->in->rdata) {
            LC_DEBUG(("Invalid data param in CMD_SET\r\n"));
            send_response(conn, INVALID_PARAM);
            return;
        }

        if (!atoull(conn->in->rextra, &val)) {
            LC_DEBUG(("Invalid timeout param in CMD_SET\r\n"));
            send_response(conn, INVALID_PARAM);
            return;
        }

        // add to cache
        ret = hset(cache, conn->in->rkey, conn->in->req_header.request.key_length, conn->in);
        if (ret == HERROR) {
            send_response(conn, OUT_OF_MEMORY);
            return;
        } else if (ret == HEXISTS) { // key exists? then force-update the data
            tab_item = hget(cache, conn->in->rkey, conn->in->req_header.request.key_length);
            assert(tab_item != NULL);
            del_cached_req(tab_item);
            tab_item->val = conn->in; // update with the new request
        }
        conn->in->can_free = 0;

        send_response(conn, SUCCESS);
        break;
    case CMD_DELETE:

        LC_DEBUG(("CMD_DELETE [%s]\r\n", conn->in->rkey));

        tab_item = hget(cache, conn->in->rkey, conn->in->req_header.request.key_length);
        if (!tab_item) {
            LC_DEBUG(("Key not found:%s\r\n", conn->in->rkey));
            send_response(conn, KEY_NOTEXISTS);
            return;
        }

        del_cached_req(tab_item);        
        hfree(cache, tab_item);

        send_response(conn, SUCCESS);
        break;
    case CMD_FLUSH_ALL:
        LC_DEBUG(("CMD_FLUSH_ALL\r\n"));

        henum(cache, flush_item_enum, NULL, 1);

        send_response(conn, SUCCESS);
        break;
    case CMD_CHG_SETTING:

        LC_DEBUG(("CHG_SETTING\r\n"));

        /* validate params */
        if (!conn->in->rdata) {
            LC_DEBUG(("(null) data param in CMD_CHG_SETTING\r\n"));
            send_response(conn, INVALID_PARAM);
            break;
        }

        /* process */
        if (strcmp(conn->in->rkey, "idle_conn_timeout") == 0) {
            if (!atoull(conn->in->rdata, &val)) {
                LC_DEBUG(("Invalid idle conn timeout param.\r\n"));
                send_response(conn, INVALID_PARAM);
                return;
            }
            LC_DEBUG(("SET idle conn timeout :%llu\r\n", (long long unsigned int)val));
            settings.idle_conn_timeout = val;
        } else {
            LC_DEBUG(("Invalid setting received :%s\r\n", conn->in->rkey));
            send_response(conn, INVALID_PARAM);
            return;
        }
        send_response(conn, SUCCESS);
        break;
    case CMD_GET_SETTING:

        LC_DEBUG(("GET_SETTING\r\n"));

        /* validate params */
        if (strcmp(conn->in->rkey, "idle_conn_timeout") == 0) {
            ival = li_malloc(sizeof(uint64_t));
            if (!ival) {
                send_response(conn, OUT_OF_MEMORY);
                return;
            }
            *ival = htonll(settings.idle_conn_timeout);
            add_response(conn, ival, sizeof(uint64_t), SUCCESS);
        } else {
            LC_DEBUG(("Invalid setting received :%s\r\n", conn->in->rkey));
            send_response(conn, INVALID_PARAM);
            return;
        }
        break;
    case CMD_GET_STATS:

        LC_DEBUG(("GET_STATS\r\n"));
        sval = li_malloc(LIGHTCACHE_STATS_SIZE);
        if (!sval) {
            send_response(conn, OUT_OF_MEMORY);
            return;
        }
        sprintf(sval,
                "mem_used:%llu\r\nmem_avail:%llu\r\nuptime:%lu\r\nversion: %0.1f Build.%d\r\n"
                "pid:%d\r\ntime:%lu\r\ncurr_items:%d\r\ncurr_connections:%llu\r\n"
                "cmd_get:%llu\r\ncmd_set:%llu\r\nget_misses:%llu\r\nget_hits:%llu\r\n"
                "bytes_read:%llu\r\nbytes_written:%llu\r\n",
                (long long unsigned int)li_memused(),
                (long long unsigned int)settings.mem_avail,
                (long unsigned int)CURRENT_TIME-stats.start_time,
                LIGHTCACHE_VERSION,
                LIGHTCACHE_BUILD,
                getpid(),
                CURRENT_TIME,
                hcount(cache),
                (long long unsigned int)stats.curr_connections,
                (long long unsigned int)stats.cmd_get,
                (long long unsigned int)stats.cmd_set,
                (long long unsigned int)stats.get_misses,
                (long long unsigned int)stats.get_hits,
                (long long unsigned int)stats.bytes_read,
                (long long unsigned int)stats.bytes_written);        
        add_response(conn, sval, LIGHTCACHE_STATS_SIZE, SUCCESS);
        break;
    default:
        LC_DEBUG(("Unrecognized command.[%d]\r\n", cmd));
        send_response(conn, INVALID_COMMAND);
        break;
    }

    return;

GET_KEY_NOTEXISTS:
    stats.get_misses++;
    send_response(conn, KEY_NOTEXISTS);
    return;
}

void disconnect_idle_conns(void)
{
    conn *conn, *next;

    conn=conns;
    while( conn != NULL && !conn->free && !conn->listening) {
        next = conn->next;
        if ((unsigned int)(CURRENT_TIME - conn->last_heard) > settings.idle_conn_timeout) {
            LC_DEBUG(("idle conn detected. idle timeout:%llu\r\n", (long long unsigned int)settings.idle_conn_timeout));
            disconnect_conn(conn);
            //TODO: move free items closer to head for faster searching for free items in make_conn
        }
        conn=next;
    }
}

socket_state read_nbytes(conn*conn, char *bytes, size_t total)
{
    unsigned int needed;
    int nbytes;

    needed = total - conn->in->rbytes;
    nbytes = read(conn->fd, &bytes[conn->in->rbytes], 1);
    if (nbytes == 0) {
        return READ_ERR;
    } else if (nbytes == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            LC_DEBUG(("socket read EWOUDLBLOCK, EAGAIN.\r\n"));
            return NEED_MORE;
        }
    }

    stats.bytes_read += nbytes;

    conn->in->rbytes += nbytes;
    if (conn->in->rbytes == total) {
        conn->in->rbytes = 0;
        return READ_COMPLETED;
    }

    return NEED_MORE;
}

int try_read_request(conn* conn)
{
    socket_state ret;

    switch(conn->state) {
    case READ_HEADER:        
        ret = read_nbytes(conn, (char *)conn->in->req_header.bytes, sizeof(req_header));
        if (ret == READ_COMPLETED) {

            /* convert network2host byte ordering before using in our code. */
            conn->in->req_header.request.data_length = ntohl(conn->in->req_header.request.data_length);
            conn->in->req_header.request.extra_length = ntohl(conn->in->req_header.request.extra_length);

            if ( (conn->in->req_header.request.data_length >= PROTOCOL_MAX_DATA_SIZE) ||
                    (conn->in->req_header.request.key_length >= PROTOCOL_MAX_KEY_SIZE) ||
                    (conn->in->req_header.request.extra_length >= PROTOCOL_MAX_EXTRA_SIZE) ) {
                LC_DEBUG(("request data or key length exceeded maximum allowed\r\n"));
                send_response(conn, INVALID_PARAM_SIZE);
                return FAILED;
            }

            // need2 read key?
            if (conn->in->req_header.request.key_length == 0) {
                set_conn_state(conn, CMD_RECEIVED);
                execute_cmd(conn);
            } else {
                set_conn_state(conn, READ_KEY);
            }
        }
        break;
    case READ_KEY:
        assert(conn->in);
        assert(conn->in->rkey);
        assert(conn->in->req_header.request.key_length);

        ret = read_nbytes(conn, conn->in->rkey, conn->in->req_header.request.key_length);

        if (ret == READ_COMPLETED) {
            if (conn->in->req_header.request.data_length == 0) {
                set_conn_state(conn, CMD_RECEIVED);
                execute_cmd(conn);
            } else {
                set_conn_state(conn, READ_DATA);
            }
        }
        break;
    case READ_DATA:
        assert(conn->in);
        assert(conn->in->rdata);
        assert(conn->in->req_header.request.data_length);

        ret = read_nbytes(conn, conn->in->rdata, conn->in->req_header.request.data_length);

        if (ret == READ_COMPLETED) {
            if (conn->in->req_header.request.extra_length) { // do we have extra data?
                set_conn_state(conn, READ_EXTRA);
            } else {
                set_conn_state(conn, CMD_RECEIVED);
                execute_cmd(conn);
            }
        }
        break;
    case READ_EXTRA:
        ret = read_nbytes(conn, conn->in->rextra, conn->in->req_header.request.extra_length);
        if (ret == READ_COMPLETED) {
            set_conn_state(conn, CMD_RECEIVED);
            execute_cmd(conn);
        }
        break;
    default:
        LC_DEBUG(("Invalid state in try_read_request\r\n"));
        ret = FAILED;
        send_response(conn, INVALID_STATE);
        break;
    } // switch(conn->state)

    return ret;
}

socket_state send_nbytes(conn*conn, char *bytes, size_t total)
{
    int needed, nbytes;

    //LC_DEBUG(("send_nbytes called.[left:%ld, fd:%d]\r\n", total - conn->out.sbytes, conn->fd));

    needed = total - conn->out.sbytes;
    nbytes = write(conn->fd, &bytes[conn->out.sbytes], 1); //needed
    if (nbytes == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return NEED_MORE;
        }
        LC_DEBUG(("socket write error.[%s]\r\n", strerror(errno)));
        syslog(LOG_ERR, "socket write error.[%s]\r\n", strerror(errno));
        return SEND_ERR;
    }

    stats.bytes_written += nbytes;

    conn->out.sbytes += nbytes;
    if (conn->out.sbytes == total) {
        conn->out.sbytes = 0;
        return SEND_COMPLETED;
    }

    return NEED_MORE;
}

int try_send_response(conn *conn)
{
    socket_state ret;

    switch(conn->state) {

    case SEND_HEADER:
        ret = send_nbytes(conn, (char *)conn->out.resp_header.bytes, sizeof(resp_header));
        if (ret == SEND_COMPLETED) {
            if (ntohl(conn->out.resp_header.response.data_length) != 0) {
                set_conn_state(conn, SEND_DATA);
            } else {
                set_conn_state(conn, CMD_SENT);
                set_conn_state(conn, READ_HEADER);// wait for new commands
            }
        }
        break;
    case SEND_DATA:
        ret = send_nbytes(conn, conn->out.sdata, ntohl(conn->out.resp_header.response.data_length));
        if (ret == SEND_COMPLETED) {
            set_conn_state(conn, CMD_SENT);
            set_conn_state(conn, READ_HEADER);// wait for new commands
        }
        break;
    default:
        LC_DEBUG(("Invalid state in try_send_response %d\r\n", conn->state));
        ret = FAILED;
        send_response(conn, INVALID_STATE);
        break;
    }

    return ret;

}

void event_handler(conn *conn, event ev)
{
    int conn_sock;
    unsigned int slen;
    struct sockaddr_in si_other;
    socket_state sock_state;

    /* check if connection is closed, this may happen where a READ and WRITE
     * event is awaiting for an fd in one cycle. Just noop for this situation.*/
    if (conn->state == CONN_CLOSED) {
        LC_DEBUG(("Connection is closed in the previous event of the cycle.\r\n"));
        return;
    }

    conn->last_heard = CURRENT_TIME;

    slen = sizeof(si_other);

    switch(ev) {
    case EVENT_READ:
        if (conn->listening) { // listening socket?
            conn_sock = accept(conn->fd, (struct sockaddr *)&si_other, &slen);
            if (conn_sock == -1) {
                syslog(LOG_ERR, "%s (%s)", "socket accept  error.", strerror(errno));
                return;
            }
            if (make_nonblocking(conn_sock)) {
                LC_DEBUG(("make_nonblocking failed.\r\n"));
            }
            conn = make_conn(conn_sock);            
            if (!conn) {
                close(conn_sock);
                return;
            }
            set_conn_state(conn, READ_HEADER);
        } else {
            sock_state = try_read_request(conn);
            if (sock_state == READ_ERR) {
                disconnect_conn(conn);
                return;
            }
        }

        break;
    case EVENT_WRITE:
        sock_state = try_send_response(conn);
        if (sock_state == SEND_ERR) {
            disconnect_conn(conn);
            return;
        }
        break;
    }
    return;
}


/* 
   This function will be called when application memory usage reaches a certain
   threshold ratio of the total available mem. Here, we will shrink static resources to gain
   more memory for dynamic resources.
  */
void collect_unused_memory(void)
{
    // todo: timedout items and free conns can be collected here.
}


static int init_server_socket(void)
{
    int s, optval, ret;
    struct sockaddr_in si_me;
    struct sockaddr_un su_me;
    struct conn *conn;
    struct stat tstat;
    struct linger ling = {0, 0};

    if (settings.socket_path) {
        // clean previous socket file.
        // todo: lstat() meybe necessary but not ANSI complaint, aslo the check
        // of S_ISSOCK() here is needed but not ANSI compliant.
        if (stat(settings.socket_path, &tstat) == 0) {
            unlink(settings.socket_path);
        }

        if ((s=socket(AF_UNIX, SOCK_STREAM, 0))==-1) {
            syslog(LOG_ERR, "%s (%s)", "unix socket make error.", strerror(errno));
            return 0;
        }
    } else {
        if ((s=socket(AF_INET, SOCK_STREAM, 0))==-1) {
            syslog(LOG_ERR, "%s (%s)", "socket make error.", strerror(errno));
            return 0;
        }
    }

    optval = 1;
    ret = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (ret != 0) {
        syslog(LOG_ERR, "setsockopt(SO_REUSEADDR) error.(%s)", strerror(errno));
    }
    ret = setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
    if (ret != 0) {
        syslog(LOG_ERR, "setsockopt(SO_KEEPALIVE) error.(%s)", strerror(errno));
    }
    ret = setsockopt(s, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
    if (ret != 0) {
        syslog(LOG_ERR, "setsockopt(SO_LINGER) error.(%s)", strerror(errno));
    }
    //ret = maximize_sndbuf(s);
    //if (!ret) {
    //    LC_DEBUG(("maximize sendbuf failed.\r\n"));
    //    syslog(LOG_ERR, "maximize_sndbuf error.(%s)", strerror(errno));
    //}

    // only for TCP sockets.
    if (!settings.socket_path) {
        ret = setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
        if (ret != 0) {
            syslog(LOG_ERR, "setsockopt(TCP_NODELAY) error.(%s)", strerror(errno));
        }
    }

    if (settings.socket_path) {
        memset((char *) &su_me, 0, sizeof(su_me));
        su_me.sun_family = AF_UNIX;
        strncpy(su_me.sun_path, settings.socket_path, sizeof(su_me.sun_path) - 1);
        if (bind(s, (struct sockaddr *)&su_me, sizeof(su_me)) == -1) {
            LC_DEBUG(("%s (%s)\r\n", "socket bind error.", strerror(errno)));
            syslog(LOG_ERR, "%s (%s)", "socket bind error.", strerror(errno));
            close(s);
            return 0;
        }
    } else {
        memset((char *) &si_me, 0, sizeof(si_me));
        si_me.sin_family = AF_INET;
        si_me.sin_port = htons(LIGHTCACHE_PORT);
        si_me.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me))==-1) {
            LC_DEBUG(("%s (%s)\r\n", "socket bind error.", strerror(errno)));
            syslog(LOG_ERR, "%s (%s)", "socket bind error.", strerror(errno));
            close(s);
            return 0;
        }
    }

    if (make_nonblocking(s)) {
        LC_DEBUG(("make_nonblocking failed.\r\n"));
    }

    if (listen(s, LIGHTCACHE_LISTEN_BACKLOG) == -1) {
        syslog(LOG_ERR, "%s (%s)", "socket listen error.", strerror(errno));
        close(s);
        return 0;
    }
    conn = make_conn(s);
    if (!conn) {
        return 0;
    }

    conn->listening = 1;
    event_set(conn, EVENT_READ);

    return 1;
}

int main(int argc, char **argv)
{
    int ret, c;
    time_t ctime, ptime;
    uint64_t param;    
    struct rlimit rlp;

    init_settings();

    /* get cmd line args */
    while (-1 != (c = getopt(argc, argv, "m: d: s: l:"))) {
        switch (c) {
        case 'm':
            ret = atoull(optarg, &param);
            if (!ret) {
                syslog(LOG_ERR, "Maximum Available Memory setting value not in range.");
                goto err;
            }
            settings.mem_avail = (param * 1024 * 1024);
            break;
        case 'd':
            settings.deamon_mode = atoi(optarg);
            break;
        case 's':
            settings.socket_path = optarg;
            break;
        case 'l':
            settings.fd_limit = atoi(optarg);
            break;
        }
    }
    
    // try to initialize the slab allocator. If slabs cannot uniformly distributed 
    // to all caches, then fallback to system's malloc 
    if (!init_cache_manager(settings.mem_avail/1024/1024, SLAB_SIZE_FACTOR)) {
        fprintf(stderr, "WARNING: falling back to system malloc.[%u,%u:%llu]\r\n", 
                slab_stats.slab_count, slab_stats.cache_count, 
                (unsigned long long)settings.mem_avail/1024/1024);
        settings.use_sys_malloc = 1;           
    } else {
        if (slab_stats.slab_count < slab_stats.cache_count) {
            fprintf(stderr, "WARNING: at least %u MB of memory " 
                "is required to utilize the slab allocator,\r\n"
                "falling back to system malloc.[%u,%u:%llu]\r\n", 
                slab_stats.cache_count, slab_stats.slab_count, slab_stats.cache_count, 
                (unsigned long long)settings.mem_avail/1024/1024);
            settings.use_sys_malloc = 1;
        } else {
            LC_DEBUG(("using slab allocator with %llu MB of memory.\r\n", 
                (unsigned long long int)settings.mem_avail/1024/1024));
        }  
    }
    
    // try to adjust system open file limit
    rlp.rlim_cur = rlp.rlim_max = settings.fd_limit; 
    if (setrlimit(RLIMIT_NOFILE, &rlp) == -1) {
        if (errno == EPERM) {
            fprintf(stderr, "WARNING: need root privieleges for changing system open file limit.\r\n");
        } else {
            fprintf(stderr, "WARNING: setrlimit(%u) failed.[%s]\r\n", 
                settings.fd_limit, strerror(errno));
        } 
    }
    LC_DEBUG(("INFO: current system open file limit is %d.\r\n", settings.fd_limit)); 
    syslog(LOG_ERR, "INFO: current system open file limit is %d.\r\n", 
            settings.fd_limit); 
    
    init_stats();

    init_log();

    if (settings.deamon_mode) {
        deamonize();
    } else {
        // When debugging with gprof, we run app in TTY and exit with CTRL+C(SIGINT)
        // this is to gracefully exit the app. Otherwise, profiling information
        // cannot be emited.
        signal(SIGINT, sig_handler);
    }

    signal(SIGPIPE, SIG_IGN);

    ret = event_init(event_handler);
    if (!ret) {
        goto err;
    }

    /* init listening socket. */
    if (!init_server_socket()) {
        goto err;
    }

    /* create the in-memory hash table. Constant is not important here.
     * Hash table is an exponantially growing as more and more items being
     * added.
     * */
    cache = htcreate(4);
    if (!cache) {
        goto err;
    }

    LC_DEBUG(("lightcache started.[%s]\r\n", settings.socket_path));

    ptime = 0;
    for (;;) {

        ctime = CURRENT_TIME;

        event_process();

        if (ctime-ptime > 1) {

            // Note: This code is executed per-sec roughly. Audits below can hold another variable to count
            // how many seconds elapsed to invoke themselves or not.

            disconnect_idle_conns();

            if ( (li_memused() * 100 / settings.mem_avail) > LIGHTCACHE_GARBAGE_COLLECT_RATIO_THRESHOLD) {
                collect_unused_memory();
            }

            ptime = ctime;
        }


    }
       
err:
    syslog(LOG_INFO, "lightcache stopped.");
    closelog();
    exit(EXIT_FAILURE);
}
