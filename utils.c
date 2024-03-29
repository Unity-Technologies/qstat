/*
 * Utility Functions
 *
 */

#include "utils.h"

#ifndef _WIN32
 #include <err.h>
 #include <sysexits.h>
#endif

#if !HAVE_STRNSTR

/*
 * strnstr -
 * Copyright (c) 2001 Mike Barcroft <mike@FreeBSD.org>
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

 #include <string.h>

	char *
	qstat_strnstr(const char *s, const char *find, size_t slen)
	{
		char c, sc;
		size_t len;

		if ((c = *find++) != '\0') {
			len = strlen(find);
			do {
				do {
					if ((slen-- < 1) || ((sc = *s++) == '\0')) {
						return (NULL);
					}
				} while (sc != c);
				if (len > slen) {
					return (NULL);
				}
			} while (strncmp(s, find, len) != 0);
			s--;
		}
		return ((char *)s);
	}


#endif  /* !HAVE_STRNSTR */

#if !HAVE_STRNDUP

 #include <stdlib.h>
 #include <string.h>

	char *
	strndup(const char *string, size_t len)
	{
		char *result;

		result = (char *)malloc(len + 1);
		memcpy(result, string, len);
		result[len] = '\0';

		return (result);
	}


#endif  /* !HAVE_STRNDUP */

#if !HAVE_ERR_H

 #include <errno.h>
 #include <stdio.h>
 #include <stdarg.h>

	void
	err(int eval, const char *fmt, ...)
	{
		va_list list;

		va_start(list, fmt);
		warn(fmt, list);
		va_end(list);

		exit(eval);
	}


	void
	warn(const char *fmt, ...)
	{
		va_list list;

		va_start(list, fmt);
		if (fmt) {
			fprintf(stderr, fmt, list);
		}
		fprintf(stderr, "%s\n", strerror(errno));
		va_end(list);
	}


#endif  /* HAVE_ERR_H */

#include <string.h>

// NOTE: replace must be smaller or equal in size to find
char *
str_replace(char *source, char *find, char *replace)
{
	char *s = strstr(source, find);
	int rlen = strlen(replace);
	int flen = strlen(find);

	if (rlen > flen) {
		err(EX_SOFTWARE, "str_replace: replace is larger than find");
	}

	while (NULL != s) {
		strncpy(s, replace, rlen); // -Wstringop-truncation warning here is a false positive.
		if (rlen < flen) {
			strcpy(s + rlen, s + flen);
		}
		s += rlen;
		s = strstr(s, find);
	}

	return (source);
}
