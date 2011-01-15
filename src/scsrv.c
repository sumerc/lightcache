
#include "scsrv.h"
#include "deamonize.h"
#include "worker.h"
#include "mem.h"
#include "hashtab.h"
#include "cache.h"
#include "socket.h"


// globals
int terminate_server = 0;
#ifdef TCP
int epollfd; // extern in header
_client *clients_head = NULL;
#endif



#ifdef TCP
static _client *
_add_client(int fd)
{
    _client *cli;

    // get a client object
    cli = (_client *)ymalloc(sizeof(_client));

    cli->fd = fd;
    cli->ridx = 0;
    cli->swnd = NULL;
    cli->sidx = 0;
    cli->slen = 0;
    cli->do_not_cache_swnd = 0;
    cli->last_heard = time(NULL);

    cli->next = (struct _client *)clients_head;
    clients_head = cli;

    dprintf("add client invoked.");

    return cli;
}

static void
_del_client(_client *cli)
{
    _client *cp, *pp;

    dprintf("del client invoked.");

    // remove client from clients list
    for (cp = clients_head; cp != NULL; pp=cp, cp=(_client *)cp->next) {
        if (cp == cli) {
            if (cp == clients_head) {
                clients_head = (_client *)cp->next;
            } else {
                pp->next = cp->next;
            }
        }
    }

    // client is in the middle of sending or completed
    // sending something? If so, update ref count of the
    // send descriptor. ((cli->sidx <= cli->slen) && )
    if (cli->swnd) {
        if (cli->do_not_cache_swnd) {
            yfree(cli->swnd);
        } else {
            dec_ref((_img *)cli->swnd);
        }
    }

    // close client socket
    yclose(cli->fd);

    // free the client
    yfree(cli);

    YMEMLEAKCHECK();

}

static void
_show_clients(void)
{
    _client *cp;

    for (cp = clients_head; cp != NULL; cp=(_client *)cp->next) {
        yinfo("cp:%p, next:%p", cp, cp->next);
    }
}

void
_idle_client_audit_func(void)
{
    _client *cp,*next;
    time_t ctime;

    dprintf("idle client audit invoked.(%p)", clients_head);

    ctime = time(NULL);
    cp = clients_head;
    while (cp) {
        next = (_client *)cp->next;
        if (ctime - cp->last_heard >= IDLE_CLIENT_TIMEOUT) {
            _del_client(cp);
        }
        cp = next;
    }
}

#endif

static int
_imgenumdel(_hitem *item, void * arg)
{
    _img *img;

    img = (_img *)item->val;
    yfree(img->data);
    yfree(img);
}


int
main(void)
{
#ifdef TCP
    struct epoll_event ev, events[MAX_EVENTS];
    int conn_sock, nfds, n, optval, tmp;
    char *picname;
    time_t prev_idle_client_audit_invoke, ctime;
    _client *client;
    _img *tmp;
#endif
    struct sockaddr_in si_me, si_other;
    int s, i, slen, nbytes,rc;
    char buf[MAXBUFLEN];
#ifdef UDP
    _cdata *cdata;
#endif

    // initialize variables
    s, i, slen=sizeof(si_other);
    rc = nbytes = 0;
    prev_idle_client_audit_invoke = 0;

    // open log deamon connection
    openlog("scsrv", LOG_PID, LOG_LOCAL5);
    syslog(LOG_INFO, "scsrv application started.");

#ifdef DEAMON
    deamonize();
#endif


// create & initialize server socket
#ifdef TCP
    if ((s=socket(AF_INET, SOCK_STREAM, 0))==-1) {
        syslog(LOG_ERR, "socket [%s]", strerror(errno));
        goto err;
    }
    optval = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
#endif
#ifdef UDP
    // create&initialize server socket
    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
        syslog(LOG_ERR, "socket [%s]", strerror(errno));
        goto err;
    }
#endif

    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me))==-1) {
        syslog(LOG_ERR, "bind [%s]", strerror(errno));
        goto err;
    }

#ifdef TCP
    ysetnonblocking(s);
    listen(s, LISTEN_BACKLOG);
    epollfd = epoll_create(MAX_EVENTS);
    if (epollfd == -1) {
        syslog(LOG_ERR, "epoll_create [%s]", strerror(errno));
        goto err;
    }

    ev.events = EPOLLIN;
    ev.data.fd = s;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, s, &ev) == -1) {
        syslog(LOG_ERR, "epoll_ctl [%s]", strerror(errno));
        goto err;
    }
#endif

    // initialize cache system hash table
    cache.tab = htcreate(IMG_CACHE_LOGSIZE);

    // create cache clean audit
    rc = create_cache_clean_audit();
    if (rc != 0) {
        syslog(LOG_ERR, "create_cache_clean_audit [%d]", rc);
        goto err;
    }

    // server loop
    for (;;) {

        if (terminate_server) {
            break;
        }

#ifdef UDP
        // This call says that we want to receive a packet from s, that the data should be put info buf, and that
        // buf can store at most BUFLEN characters. The zero parameter says that no special flags should be used.
        // Data about the sender should be stored in si_other, which has room for slen byte. Note that recvfrom()
        // will set slen to the number of bytes actually stored. If you want to play safe, set slen to
        // sizeof(si_other) after each call to recvfrom().
        nbytes = recvfrom(s, buf, MAXBUFLEN, 0, (struct sockaddr *)&si_other, &slen);
        if (nbytes ==-1) {
            syslog(LOG_ERR, "recvfrom [%s]", strerror(errno));
            continue;
        }

        // copy locals to dynamic variables for the worker threads. We do not
        // check for the overflow of buf as recvfrom() will not recv more than MAXBUFLEN
        // bytes of data. It will block.
        cdata = (_cdata *)ymalloc(sizeof(_cdata));
        strncpy(cdata->cmd, buf, nbytes);
        cdata->cmd[nbytes] = 0; // paranoid assignment
        memcpy(&cdata->saddr, &si_other, slen);

        // Worker thread, _if not failed_ is responsible for freeing the
        // resource.
        rc = create_worker((void *)cdata);
        if (rc != 0) {
            syslog(LOG_ERR, "create_worker [%d]", rc);
            yfree(cdata);
            continue;
        }
#endif

#ifdef TCP

        nfds = epoll_wait(epollfd, events, MAX_EVENTS, EPOLL_TIMEOUT);
        if (nfds == -1) {
            syslog(LOG_ERR, "epoll_wait [%s]", strerror(errno));
            continue;
        }

        // invoke idle client audit?
        ctime = time(NULL);
        if (ctime - prev_idle_client_audit_invoke >= IDLE_CLIENT_AUDIT_INTERVAL) {
            _idle_client_audit_func();
            prev_idle_client_audit_invoke = ctime;
            continue; // there may be some disconnected users, so continue...
        }

        for (n = 0; n < nfds; ++n) {
            if (events[n].data.fd == s) {
                conn_sock = accept(s, (struct sockaddr *)&si_other, &slen);
                if (conn_sock == -1) {
                    syslog(LOG_ERR, "accept [%s]", strerror(errno));
                    continue;
                }

                ysetnonblocking(conn_sock);

                ev.events = EPOLLIN;
                ev.data.ptr = _add_client(conn_sock);
                ychg_interest(conn_sock, EPOLL_CTL_ADD, ev);

            } else {
                // get the client data from epoll_user_data from the current event
                // set.
                client = (_client *)events[n].data.ptr;
                if (IS_READABLE(events[n])) {
                    rc = yrecv(client->fd, &client->rwnd[client->ridx], RECV_CHUNK_SIZE-1);
                    if (rc == -1) {
                        _del_client(client);
                        continue;
                    }

                    client->ridx += rc;
                    client->last_heard = time(NULL);

                    // have we received all the cmd?
                    if (client->rwnd[client->ridx-1] != '\n') {
                        // buffer overflow can happen? if so continue
                        // with the partial message received
                        if (client->ridx+RECV_CHUNK_SIZE >= MAXBUFLEN) {
                            ;
                        } else {
                            continue;
                        }

                    }
                    client->rwnd[client->ridx-1] = (char)0;

                    // command control.
                    if (strncmp(client->rwnd, "TMB", 3) == 0) {

                        // get the image file.
                        picname = &client->rwnd[4];

                        // create/init image buf for sending
                        client->slen = get_or_insert_img(picname, &client->swnd);
                        if (!client->slen) {
                            _del_client(client);
                            continue;
                        }

                        // change interest for sending
                        ev.events = EPOLLOUT;
                        if (ychg_interest(client->fd, EPOLL_CTL_MOD, ev) == -1) {
                            syslog(LOG_ERR, "epoll_wait [%s]", strerror(errno));
                            _del_client(client);
                            continue;
                        }

                    } else if (strncmp(client->rwnd, "GET", 3) == 0) {
                        // port is requested from a browser? then
                        // send cache statistics back by updating send
                        // window of the client
                        client->slen = get_cache_statistics(&buf[0]);
                        client->do_not_cache_swnd = 1; // swnd will be freed after data is sent.
                        tmp = (_img *)ymalloc(sizeof(_img));
                        tmp->data = buf;
                        client->swnd = (char *)tmp;

                        ev.events = EPOLLOUT;
                        if (ychg_interest(client->fd, EPOLL_CTL_MOD, ev) == -1) {
                            syslog(LOG_ERR, "epoll_wait [%s]", strerror(errno));
                            _del_client(client);
                            continue;
                        }
                    } else if (strncmp(client->rwnd, "TERM", 4) == 0) {
                        _del_client(client);
                        terminate_server = 1;
                        continue;
                    } else {
                        dprintf("msg received: %s", client->rwnd);
                    }
                }
                if (IS_WRITEABLE(events[n])) {
                    rc = ysend(client->fd, &((_img *)client->swnd)->data[client->sidx], client->slen);
                    if (rc == -1) {
                        _del_client(client);
                        continue;
                    }
                    client->sidx +=rc;

                    dprintf("sidx:%d, slen:%d", client->sidx, client->slen);

                    // operation completed?
                    if (client->sidx >= client->slen) {
                        _del_client(client);
                        continue;
                    }

                    client->last_heard = time(NULL);
                }
            }
        }
#endif
    } // server for-loop

    dprintf("Closing audit...");

    // first close the clean audit
    CACHE_LOCK;
    cache.terminate = 1;
    CACHE_UNLOCK;
    pthread_join(cache.thread, NULL);

    dprintf("Freeing resources...");

    // now, as we close the cache clean audit, all below is thread safe
    closelog(); // close connection to the log deamon
    henum(cache.tab, _imgenumdel, NULL, 1); // enums all - including free items
    htdestroy(cache.tab); // free cache

    dprintf("Application closed.");
    syslog(LOG_ERR, "scsrv application closed.", rc);

#ifdef TCP
    yclose(s); // close listen socket
#endif
    YMEMLEAKCHECK();
    return 0;
err:
    closelog();
    exit(EXIT_FAILURE);
}
