/*
 * Utility Functions
 * Copyright 2012 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_UTILS_H
#define QSTAT_UTILS_H
#include "gnuconfig.h"

// BSD has strnstr
#if defined(__FreeBSD__) || defined(__MidnightBSD__) || defined(__OpenBSD__)
#ifndef HAVE_STRNSTR
#define HAVE_STRNSTR 1
#endif /* HAVE_STRNSTR */
#endif

#ifndef _WIN32
#ifndef HAVE_ERR_H
#define HAVE_ERR_H 1
#endif /* HAVE_ERR */
#endif

#if !HAVE_STRNSTR
#include <string.h>
char * qstat_strnstr(const char *s, const char *find, size_t slen);
#define strnstr(s,find,slen) qstat_strnstr(s,find,slen)
#endif

#if !HAVE_STRNDUP
#include <string.h>
char *strndup(const char *string, size_t len);
#endif

#ifndef EX_OSERR
#define EX_OSERR 71  /* system error (e.g., can't fork) */
#endif

#if !HAVE_ERR_H
void err(int eval, const char *fmt, ...); 
void warn(const char *fmt, ...);
#endif

#if defined(_MSC_VER) && _MSC_VER < 1600
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#elif defined(_MSC_VER)  // && _MSC_VER >= 1600
#include <stdint.h>
#else
#include <stdint.h>
#endif

char *str_replace(char *, char *, char *);

#endif
