#include "lightcache.h"
#include "protocol.h"
#include "socket.h"
#include "hashtab.h"
#include "freelist.h"
#include "event.h"
#include "mem.h"
#include "util.h"

/* forward declarations */
void set_conn_state(struct conn* conn, conn_states state);

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
    settings.deamon_mode = 1;
    settings.mem_avail = 64 * 1024 * 1024; // in bytes -- 64 mb -- default same as memcached
}

void
init_log(void)
{
    openlog("lightcache", LOG_PID, LOG_LOCAL5);
    syslog(LOG_INFO, "lightcache started.");
}

void
init_stats(void)
{
    stats.mem_used = 0;
    stats.mem_request_count = 0;
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
        fprintf(stderr, "FREEING request data.[%p], sizeof:[%lu]\r\n", (void *)req, sizeof(request *));
        li_free(req->rkey);
        li_free(req->rdata);
        li_free(req->rextra);
        req->rkey = NULL;
        req->rdata = NULL;
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

    if (resp->can_free && resp->sdata) {
        fprintf(stderr, "FREEING response data.[%p]\r\n", (void *)resp);
        li_free(resp->sdata);
    }

    /* can lose reference to mem, otherwise, later freelist usage of the same resp
     * object may lead to invalid deletion of this data.
     * */
    resp->sdata = NULL;

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
    conn->in->can_free = 1;
    conn->out->can_free = 1;
    conn->in->rbytes = 0;
    conn->out->sbytes = 0;

    return 1;
}

static void
disconnect_conn(conn* conn)
{
    fprintf(stderr, "disconnect conn called.\r\n");

    event_del(conn);

    free_request(conn->in);
    free_response(conn->out);

    conn->free = 1;
    close(conn->fd);

    set_conn_state(conn, CONN_CLOSED);
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
        conn->in->rkey = (char *)li_malloc(conn->in->req_header.request.key_length + 1);
        if (!conn->in->rkey) {
            disconnect_conn(conn);
            return;
        }
        conn->in->rkey[conn->in->req_header.request.key_length] = (char)0;
        break;
    case READ_DATA:
        conn->in->rdata = (char *)li_malloc(conn->in->req_header.request.data_length + 1);
        if (!conn->in->rdata) {
            disconnect_conn(conn);
            return;
        }
        conn->in->rdata[conn->in->req_header.request.data_length] = (char)0;
        break;
    case READ_EXTRA:
        conn->in->rextra = (char *)li_malloc(conn->in->req_header.request.extra_length + 1);
        if (!conn->in->rextra) {
            disconnect_conn(conn);
            return;
        }
        conn->in->rextra[conn->in->req_header.request.extra_length] = (char)0;
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
prepare_response(conn *conn, size_t data_length, int alloc_mem)
{
    assert(conn->out != NULL);

    if (alloc_mem) {
        conn->out->sdata = (char *)li_malloc(data_length);
        if (!conn->out->sdata) {
            disconnect_conn(conn);
            return 0;
        }
    }
    conn->out->resp_header.response.data_length = htonl(data_length);
    conn->out->resp_header.response.opcode = conn->in->req_header.request.opcode;

    return 1;
}

void
execute_cmd(struct conn* conn)
{
    int r;
    hresult ret;
    uint8_t cmd;
    uint64_t val;
    request *cached_req;
    _hitem *tab_item;

    assert(conn->state == CMD_RECEIVED);

    /* here, the complete request is received from the connection */
    conn->in->received = time(NULL);

    cmd = conn->in->req_header.request.opcode;

    switch(cmd) {
    case CMD_GET:

        fprintf(stderr, "CMD_GET request for key: %s\r\n", conn->in->rkey);
        tab_item = hget(cache, conn->in->rkey, conn->in->req_header.request.key_length);
        if (!tab_item) {
            fprintf(stderr, "key not found:%s\r\n", conn->in->rkey);
            return;
        }
        cached_req = (request *)tab_item->val;

        /* check timeout expire */
        r = atoull(cached_req->rextra, &val);
        assert(r != 0);/* CMD_SET already does this validation but re-check*/

        if ( (unsigned int)(time(NULL)-cached_req->received) > val) {
            fprintf(stderr, "time expired for key:%s\r\n", conn->in->rkey);
            cached_req->can_free = 1;
            free_request(cached_req);
            hfree(cache, tab_item); // recycle tab_item
            disconnect_conn(conn);
            return;
        }
        if (!prepare_response(conn, cached_req->req_header.request.data_length, 0)) { // do not alloc mem
            return;
        }
        conn->out->sdata = cached_req->rdata;
        conn->out->can_free = 0;

        fprintf(stderr, "sending GET data:%s\r\n", conn->out->sdata);

        set_conn_state(conn, SEND_HEADER);
        break;

    case CMD_SET:
        fprintf(stderr, "CMD_SET request for key, data, extra: %s, %s, %s\r\n", conn->in->rkey, conn->in->rdata,
                conn->in->rextra);

        /* validate params */
        if (!conn->in->rkey) {
            fprintf(stderr, "invalid key param in CMD_SET\r\n");
            return;
        }
        if (!conn->in->rdata) {
            fprintf(stderr, "invalid data param in CMD_SET\r\n");
            return;
        }
        
        if (!atoull(conn->in->rextra, &val)) {
            fprintf(stderr, "invalid timeout param in CMD_SET\r\n");
            disconnect_conn(conn);
            return;
        }        

        /* add to cache */
        ret = hset(cache, conn->in->rkey, conn->in->req_header.request.key_length, conn->in);
        if (ret == HEXISTS) { // key exists? then force-update the data
            tab_item = hget(cache, conn->in->rkey, conn->in->req_header.request.key_length);
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
        fprintf(stderr, "CMD_CHG_SETTING request with data %s, key_length:%d",
                conn->in->rdata, conn->in->req_header.request.key_length);
        if (!conn->in->rdata) {
            fprintf(stderr, "(null) data param in CMD_CHG_SETTING\r\n");
            break;
        }

        if (strcmp(conn->in->rkey, "idle_conn_timeout") == 0) {
            if (!atoull(conn->in->rdata, &val)) {
                fprintf(stderr, "invalid idle conn timeout param.\r\n");
                disconnect_conn(conn);
                return;
            }
            fprintf(stderr, "SET idle conn timeout :%llu\r\n", (long long unsigned int)val);
            settings.idle_conn_timeout = val;
        } else if (strcmp(conn->in->rkey, "mem_avail") == 0) {
            if (!atoull(conn->in->rdata, &val)) {
                fprintf(stderr, "invalid mem avail param.\r\n");
                disconnect_conn(conn);
                return;
            }
            fprintf(stderr, "SET mem avail :%llu", (long long unsigned int)val);
            settings.mem_avail = val * 1024 * 1024; /*todo:can overflow*/
        }
        set_conn_state(conn, READ_HEADER);
        break;
    case CMD_GET_SETTING:
        fprintf(stderr, "CMD_GET_SETTING request for key: %s, data:%s", conn->in->rkey,
                conn->in->rdata);
        if (strcmp(conn->in->rkey, "idle_conn_timeout") == 0) {
            if (!prepare_response(conn, sizeof(uint64_t), 1)) {
                return;
            }
            *(uint64_t *)conn->out->sdata = htonll(settings.idle_conn_timeout);
            set_conn_state(conn, SEND_HEADER);
        } else if (strcmp(conn->in->rkey, "mem_avail") == 0) {
            if (!prepare_response(conn, sizeof(uint64_t), 1)) {
                return;
            }
            *(uint64_t *)conn->out->sdata = htonll(settings.mem_avail / 1024 / 1024);
            set_conn_state(conn, SEND_HEADER);
        }
        break;
    case CMD_GET_STATS:
        fprintf(stderr, "CMD_GET_STATS request");
        if (!prepare_response(conn, 250, 1)) {
            return;
        }
        sprintf(conn->out->sdata, "mem_used:%llu\r\n", (long long unsigned int)stats.mem_used);
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

        //fprintf(stderr, ">>>>>>>>>>>>>>>idle timeout:%llu, timediff=%d\r\n", settings.idle_conn_timeout,
        //	time(NULL) - conn->last_heard);

        if ((unsigned int)(time(NULL) - conn->last_heard) > settings.idle_conn_timeout) {
            fprintf(stderr, "idle conn detected. idle timeout:%llu\r\n", (long long unsigned int)settings.idle_conn_timeout);
            disconnect_conn(conn);
            // todo: move free items closer to head for faster searching for free items in make_conn
        }
        conn=next;
    }
}

socket_state
read_nbytes(conn*conn, char *bytes, size_t total)
{
    unsigned int needed;
    int nbytes;

    fprintf(stderr, "read_nbytes called.\r\n");

    needed = total - conn->in->rbytes;
    nbytes = read(conn->fd, &bytes[conn->in->rbytes], needed);
    if (nbytes == 0) {
        syslog(LOG_ERR, "%s (%s)", "socket read error.\r\n", strerror(errno));
        return READ_ERR;
    } else if (nbytes == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            fprintf(stderr, "socket read EWOUDLBLOCK, EAGAIN.\r\n");
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
                syslog(LOG_ERR, "request data or key length exceeded maximum allowed %u.", PROTOCOL_MAX_DATA_SIZE);
                fprintf(stderr, "request data or key length exceeded maximum allowed\r\n");
                return READ_ERR;
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

            // need2 read data?
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
        fprintf(stderr, "Invalid state in try_read_request\r\n");
        ret = INVALID_STATE;
        break;
    } // switch(conn->state)

    return ret;
}

socket_state
send_nbytes(conn*conn, char *bytes, size_t total)
{
    int needed, nbytes;

    fprintf(stderr, "send_nbytes called.\r\n");

    needed = total - conn->out->sbytes;
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
            if (ntohl(conn->out->resp_header.response.data_length) != 0) {
                set_conn_state(conn, SEND_DATA);
            }
        }
        break;
    case SEND_DATA:
        ret = send_nbytes(conn, conn->out->sdata, ntohl(conn->out->resp_header.response.data_length));
        if (ret == SEND_COMPLETED) {
            if (ntohl(conn->out->resp_header.response.data_length) != 0) {
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
    int conn_sock;
    unsigned int slen;
    struct sockaddr_in si_other;
    socket_state sock_state;

    /* check if connection is closed, this may happen where a READ and WRITE
     * event is awaiting for an fd in one cycle. Just noop for this situation.*/
    if (conn->state == CONN_CLOSED) {
        fprintf(stderr, "Connection is closed in the previous event of the cycle.\r\n");
        return;
    }

    conn->last_heard = time(NULL);

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
                return;
            }
        }

        break;
    case EVENT_WRITE:
        sock_state = try_send_response(conn);
        if (sock_state == SEND_ERR || sock_state == INVALID_STATE) {
            disconnect_conn(conn);
            return;
        }
        break;
    }
    return;
}

/* Demands for memory that is freed but used. This may be an expired cached request
 * or a freed connection, or in freelist items. This function will be called when
 * application memory usage reaches a certain ratio of the total available mem. The
 * logic is to use every chance to respond to SET requests properly.
 * */
void
collect_unused_memory(void)
{
}

int
main(int argc, char **argv)
{
    int s, optval, ret, c;
    struct sockaddr_in si_me;
    struct conn *conn;
    time_t ctime, ptime;
    uint64_t param;

    init_settings();

    /* get cmd line args */
    while (-1 != (c = getopt(argc, argv, "m: d:"
                            ))) {
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
        }
    }

    init_stats();

    init_freelists();

    init_log();

    if (settings.deamon_mode) {
        ;
        //deamonize();
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

    /* create the in-memory hash table. Constant is not important here.
     * Hash table is an exponantially growing as more and more items being
     * added.
     * */
    cache = htcreate(4);
    if (!cache) {
        goto err;
    }

    ptime = 0;
    for (;;) {

        ctime = time(NULL);

        event_process();

        if (ctime-ptime > LIGHTCACHE_TIMEDRUN_INVOKE_INTERVAL) { // invoke per-sec

            disconnect_idle_conns();

            if ( (stats.mem_used * 100 / settings.mem_avail) > LIGHTCACHE_GARBAGE_COLLECT_RATIO_THRESHOLD) {
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
