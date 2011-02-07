
#include "util.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

static void
_child_handler(int signum)
{
    switch(signum) {
    case SIGALRM:
        exit(EXIT_FAILURE);
        break;
    case SIGUSR1:
        exit(EXIT_SUCCESS);
        break;
    case SIGCHLD:
        exit(EXIT_FAILURE);
        break;
    }
}


void
deamonize(void)
{
    pid_t pid, sid, parent;
    FILE *dummy;

    /* already a daemon */
    if ( getppid() == 1 ) return;

    /* Trap signals that we expect to receive */
    signal(SIGCHLD,_child_handler);
    signal(SIGUSR1,_child_handler);
    signal(SIGALRM,_child_handler);

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "fork #1[%s]", strerror(errno));
        closelog();
        exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then we can exit the parent process. */
    if (pid > 0) {

        /* Wait for confirmation from the child via SIGTERM or SIGCHLD, or
               for two seconds to elapse (SIGALRM).  pause() should not return. */
        alarm(2);
        pause();

        exit(EXIT_FAILURE);
    }

    /* At this point we are executing as the child process */
    parent = getppid();

    /* Cancel certain signals */
    signal(SIGCHLD,SIG_DFL); /* A child process dies */
    signal(SIGTSTP,SIG_IGN); /* Various TTY signals */
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
    signal(SIGHUP, SIG_IGN); /* Ignore hangup signal */
    signal(SIGTERM,SIG_DFL); /* Die on SIGTERM */

    /* Change the file mode mask */
    umask(0);

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        syslog(LOG_ERR, "setsid[%s]", strerror(errno));
        closelog();
        exit(EXIT_FAILURE);
    }

    /* Change the current working directory.  This prevents the current
           directory from being locked; hence not being able to remove it. */
    if ((chdir("/")) < 0) {
        syslog(LOG_ERR, "chdir[%s]", strerror(errno));
        closelog();
        exit(EXIT_FAILURE);
    }

    /* Redirect standard files to /dev/null */
    dummy = freopen( "/dev/null", "r", stdin);
    dummy = freopen( "/dev/null", "w", stdout);
    dummy = freopen( "/dev/null", "w", stderr);

    /* Tell the parent process that we are A-okay */
    //kill( parent, SIGUSR1 );
}

static uint64_t
li_swap64(uint64_t in)
{

#ifdef LITTLE_ENDIAN
    /* Little endian, flip the bytes around until someone makes a faster/better
    * way to do this. */
    int64_t rv = 0;
    int i = 0;
    for(i = 0; i<8; i++) {
        rv = (rv << 8) | (in & 0xff);
        in >>= 8;
    }
    return rv;
#else
    /* big-endian machines don't need byte swapping */
    return in;
#endif
}

uint64_t ntohll(uint64_t val)
{
    return li_swap64(val);
}

uint64_t htonll(uint64_t val)
{
    return li_swap64(val);
}

int
atoull(const char *s, uint64_t *ret)
{
    errno = 0;
    *ret = strtoull(s, NULL, 10);
    if (errno == ERANGE || errno == EINVAL) {
        return 0;
    }
    
    if (*ret == 0) { /*TODO: platform independent way to detect invalid values for an integer*/
    	return 0;
    }
    
    return 1;
}
