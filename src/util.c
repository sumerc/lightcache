
#include "util.h"
#include "config.h"

#ifdef LC_TEST
#include "assert.h"
#endif

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

/* handle signals and exit the application gracefully */
void sig_handler(int signum)
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
    case SIGINT:
        exit(EXIT_FAILURE);
        break;
    }
}


void deamonize(void)
{
    pid_t pid, sid, parent;
    FILE *dummy;

    /* already a daemon */
    if ( getppid() == 1 ) return;

    /* Trap signals that we expect to receive */
    signal(SIGCHLD, sig_handler);
    signal(SIGUSR1, sig_handler);
    signal(SIGALRM, sig_handler);
    //signal(SIGFPE, SIG_IGN); comment in after extensive testing.

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

static uint64_t li_swap64(uint64_t in)
{
#ifdef LC_LITTLE_ENDIAN
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

int atoull(const char *s, uint64_t *ret)
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

#ifdef LC_TEST
/* See if compile time params are really set correctly */
void test_endianness(void)
{
    union {
        uint32_t i;
        uint8_t c[4];
    } bint = {0x01020304};  
  
#ifdef LC_LITTLE_ENDIAN
    assert(bint.c[0] == 4);
#elif LC_BIG_ENDIAN
    assert(bint.c[0] == 1);
#endif
}

void test_util_routines(void)
{
    union {
        uint64_t i;
        uint8_t c[8];
    } u64_test = {0x0102030405060708U};
    uint8_t uch;
    int ret;

#ifdef LC_LITTLE_ENDIAN
    uch = u64_test.c[0];
    u64_test.i = ntohll(u64_test.i);
    assert(uch == u64_test.c[7]);
#elif LC_BIG_ENDIAN
    uch = u64_test.c[0];
    u64_test.i = ntohll(u64_test.i);
    assert(uch == u64_test.c[0]); // same as before
#endif
    ret = atoull("1234567891234567891234", &u64_test.i); //out of bounds
    assert(ret == 0);
    
    // test string conv is correct
    ret = atoull("18446744073709551615", &u64_test.i); // max uint64
    assert(ret == 1);
    uch = u64_test.c[0];
    u64_test.i = 18446744073709551615U;    
    assert(uch == u64_test.c[0]); 
}


#endif
