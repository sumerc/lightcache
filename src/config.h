#ifndef CONFIG_H
#define CONFIG_H

/*
 Copyright 2005 Caleb Epstein
 Copyright 2006 John Maddock
 Copyright 2010 Rene Rivera
 Distributed under the Boost Software License, Version 1.0. (See accompany-
 ing file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#if defined (__GLIBC__)
# include <endian.h>
# if (__BYTE_ORDER == __LITTLE_ENDIAN)
#   define LC_LITTLE_ENDIAN
# elif (__BYTE_ORDER == __BIG_ENDIAN)
#   define LC_BIG_ENDIAN
# else
#   error Unknown machine endianness detected.
# endif
#elif defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN) || \
    defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
# define LC_BIG_ENDIAN
#elif defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN) || \
    defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
# define LC_LITTLE_ENDIAN
#elif defined(__sparc) || defined(__sparc__) \
   || defined(_POWER) || defined(__powerpc__) \
   || defined(__ppc__) || defined(__hpux) || defined(__hppa) \
   || defined(_MIPSEB) || defined(_POWER) \
   || defined(__s390__)
# define LC_BIG_ENDIAN
#elif defined(__i386__) || defined(__alpha__) \
   || defined(__ia64) || defined(__ia64__) \
   || defined(_M_IX86) || defined(_M_IA64) \
   || defined(_M_ALPHA) || defined(__amd64) \
   || defined(__amd64__) || defined(_M_AMD64) \
   || defined(__x86_64) || defined(__x86_64__) \
   || defined(_M_X64) || defined(__bfin__)
# define LC_LITTLE_ENDIAN
#else
# error Cannot determine endianness for your CPU.
#endif

/* define polling API */
#ifdef __linux__
#define HAVE_EPOLL 1
#endif

#if (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
#define HAVE_KQUEUE 1
#endif

#endif
