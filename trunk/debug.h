#ifndef YDEBUG_H
#define YDEBUG_H
#include "stdio.h"

#ifdef DEBUG
#ifndef _MSC_VER
#define dprintf(fmt, args...) fprintf(stderr, "[&] [dbg] " fmt "\n", ## args)
#else
#define dprintf(fmt, ...) fprintf(stderr, "[&] [dbg] " fmt "\n", __VA_ARGS__)
#endif
#else
#ifndef _MSC_VER
#define dprintf(fmt, args...)
#else
#define dprintf(fmt, ...)
#endif
#endif

#ifndef _MSC_VER
#define yerr(fmt, args...) fprintf(stderr, "[*]	[err]	" fmt "\n", ## args)
#define yinfo(fmt, args...) fprintf(stderr, "[+] [info] " fmt "\n", ## args)
#define yprint(fmt, args...) fprintf(stderr, fmt,  ## args)
#else
#define yerr(fmt, ...) fprintf(stderr, "[*]	[err]	" fmt "\n", __VA_ARGS__)
#define yinfo(fmt, ...) fprintf(stderr, "[+] [info] " fmt "\n", __VA_ARGS__)
#define yprint(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#endif

#endif
