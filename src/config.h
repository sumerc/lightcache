#ifndef CONFIG_H
#define CONFIG_H

/* test for polling API */
#ifdef __linux__
#define HAVE_EPOLL 1
#endif

#if defined ((__APPLE__) && (__MACH__)) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
#define HAVE_KQUEUE 1
#endif

#endif
