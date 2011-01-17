
#include "lightcache.h"
#include "deamonize.h"
#include "worker.h"
#include "hashtab.h"
#include "cache.h"
#include "socket.h"


// globals
int terminate_server = 0;
int epollfd; // extern in header
_client *clients_head = NULL;

static _client *
_add_client(int fd)
{
    _client *cli;

    // get a client object
    cli = (_client *)ymalloc(sizeof(_client));

    cli->fd = fd;scip
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

int
main(void)
{
    struct epoll_event ev, events[MAX_EVENTS];
    int conn_sock, nfds, n, optval, tmp;
    
    time_t prev_idle_client_audit_invoke, ctime;
    _client *client;
    _img *tmp;

    struct sockaddr_in si_me, si_other;
    int s, i, slen, nbytes,rc;
    char buf[MAXBUFLEN];


    // initialize variables
    s, i, slen=sizeof(si_other);
    rc = nbytes = 0;
    prev_idle_client_audit_invoke = 0;

    // open log deamon connection
    openlog("lightcache", LOG_PID, LOG_LOCAL5);
    syslog(LOG_INFO, "lightcache started.");

#ifdef DEAMON
    deamonize();
#endif


// create & initialize server socket

    if ((s=socket(AF_INET, SOCK_STREAM, 0))==-1) {
        syslog(LOG_ERR, "socket [%s]", strerror(errno));
        goto err;
    }
    optval = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me))==-1) {
        syslog(LOG_ERR, "bind [%s]", strerror(errno));
        goto err;
    }

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

    yclose(s); // close listen socket

    YMEMLEAKCHECK();
    return 0;
err:
    closelog();
    exit(EXIT_FAILURE);
}
