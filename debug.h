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

// NOTE: Windows doesn't support debugging ATM
#ifdef _WIN32
	#define debug 0 &&
#else
	#ifdef DEBUG
		#include <stdarg.h>
		void _debug(const char* file, int line, const char* function, int level, const char* fmt, ...)
			GCC_FORMAT_PRINTF(5, 6);
		#define debug(level,fmt,rem...) \
		  if( level <= get_debug_level() ) \
			_debug(__FILE__,__LINE__,__FUNCTION__,level,fmt,##rem)
	#else
		#define debug(...)
	#endif
#endif

void dump_packet(const char* buf, int buflen);

/** report a packet decoding error to stderr */
void malformed_packet(const struct qserver* server, const char* fmt, ...) GCC_FORMAT_PRINTF(2, 3);

int get_debug_level (void);
void set_debug_level (int level);

#endif
