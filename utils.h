/*
 * Utility Functions
 * Copyright 2012 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_UTILS_H
#define QSTAT_UTILS_H

// BSD has strnstr
#if defined(__FreeBSD__) || defined(__MidnightBSD__) || defined(__OpenBSD__)
#ifndef HAVE_STRNSTR
#define HAVE_STRNSTR 1
#endif /* HAVE_STRNSTR */
#endif

#if !HAVE_STRNSTR
#include <string.h>
char * qstat_strnstr(const char *s, const char *find, size_t slen);
#define strnstr(s,find,slen) qstat_strnstr(s,find,slen)
#endif

#endif
