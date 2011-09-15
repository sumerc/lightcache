
#include "test_base.h"
#include "sys/time.h"
#include "stdio.h"
#include "assert.h"

long long g_t0 = 0;

static long long tickcount(void)
{
    struct timeval tv;
    long long rc;

    gettimeofday(&tv, (struct timezone *)NULL);

    rc = tv.tv_sec;
    rc = rc * 1000000 + tv.tv_usec;
    return rc;
}

void TEST_START(void)
{
    g_t0 = tickcount();
}

void TEST_END(const char*name)
{
    assert(g_t0 != 0);

    fprintf(stderr,
            "[+]    %s. (elapsed:%0.6f)\r\n", name, (tickcount()-g_t0)*0.000001);
    g_t0 = 0;
}
