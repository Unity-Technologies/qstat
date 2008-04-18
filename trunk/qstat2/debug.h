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

/** report a packet decoding error to stderr */
void malformed_packet(const struct qserver* server, const char* fmt, ...) GCC_FORMAT_PRINTF(2, 3);

int get_debug_level (void);
void set_debug_level (int level);

void print_packet( struct qserver *server, const char *buf, int buflen );
void output_packet( struct qserver *server, const char *buf, int buflen, int to );

#endif
