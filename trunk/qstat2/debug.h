/*
 * qstat 2.6
 * by Steve Jankowski
 *
 * debug helper functions
 * Copyright 2004 Ludwig Nussel
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_DEBUG_H
#define QSTAT_DEBUG_H

#include <sys/types.h>

#include "qstat.h"


#ifdef DEBUG
	#include <stdarg.h>
	#ifdef _WIN32
		void _debug(const char* file, int line, const char* function, int level, const char* fmt, ...);
		#define debug(level,fmt,...) \
			if( level <= get_debug_level() ) \
				_debug(__FILE__,__LINE__,__FUNCTION__,level,fmt,__VA_ARGS__)
	#else
		void _debug(const char* file, int line, const char* function, int level, const char* fmt, ...)
		GCC_FORMAT_PRINTF(5, 6);
		#define debug(level,fmt,rem...) \
			if( level <= get_debug_level() ) \
				_debug(__FILE__,__LINE__,__FUNCTION__,level,fmt,##rem)
	#endif // _WIN32
#else
	#define debug(...)
#endif // DEBUG

void dump_packet(const char* buf, int buflen);

#ifdef ENABLE_DUMP
#ifndef _WIN32
	#include <sys/mman.h>
	#include <unistd.h>
#else
	#ifdef _MSC_VER
		#define ssize_t SSIZE_T
		#define uint32_t UINT32
	#endif
#endif
#include <sys/types.h>
#include <sys/stat.h>
int do_dump;
ssize_t send_dump(int s, const void *buf, size_t len, int flags);
#ifndef QSTAT_DEBUG_C
#define send(s, buf, len, flags) send_dump(s, buf, len, flags)
#endif
#endif

/** report a packet decoding error to stderr */
void malformed_packet(const struct qserver* server, const char* fmt, ...) GCC_FORMAT_PRINTF(2, 3);

int get_debug_level (void);
void set_debug_level (int level);

void print_packet( struct qserver *server, const char *buf, int buflen );
void output_packet( struct qserver *server, const char *buf, int buflen, int to );

#endif
