/*
 * qstat
 * by Steve Jankowski
 *
 * debug helper functions
 * Copyright 2004 Ludwig Nussel
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */

#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/param.h>
#else
#include <stdarg.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <stdarg.h>

#define QSTAT_DEBUG_C
#include "debug.h"

#ifdef ENABLE_DUMP
#ifndef _WIN32
	#include <sys/socket.h>
#else
	#include <winsock.h>
#endif
#endif

#ifdef DEBUG

void _debug(const char* file, int line, const char* function, int level, const char* fmt, ...)
{
	va_list ap;
	char buf[9];
	time_t now = time( NULL );

	strftime(buf,9,"%H:%M:%S", localtime( &now ) );

	fprintf(stderr, "debug(%d) %s %s:%d %s() - ", level, buf, file, line, function);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if ( '\n' != fmt[strlen(fmt)-1]  )
	{
		fputs("\n", stderr);
	}
	return;
}
#endif

static int debug_level = 0;

void set_debug_level (int level)
{
  debug_level = level;
}

int get_debug_level (void)
{
  return (debug_level);
}

void malformed_packet(const struct qserver* server, const char* fmt, ...)
{
	va_list ap;

	if(!show_errors) return;

	fputs("malformed packet", stderr);

	if(server)
	{
		fprintf(stderr, " from %d.%d.%d.%d:%hu",
			server->ipaddr&0xff,
			(server->ipaddr>>8)&0xff,
			(server->ipaddr>>16)&0xff,
			(server->ipaddr>>24)&0xff,
			server->port);
	}
	fputs(": ", stderr);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if(*fmt && fmt[strlen(fmt)-1] != '\n')
	{
		fputc('\n',stderr);
	}
}

#ifdef ENABLE_DUMP
static unsigned count = 0;
static void _dump_packet(const char* tag, const char* buf, int buflen)
{
	char fn[PATH_MAX] = {0};
	int fd;
	sprintf(fn, "%03u_%s.pkt", count++, tag);
	fprintf(stderr, "dumping to %s\n", fn);
	fd = open(fn, O_WRONLY|O_CREAT|O_EXCL, 0644);
	if(fd == -1) { perror("open"); return; }
	if(write(fd, buf, buflen) == -1)
		perror("write");
	close(fd);
}

ssize_t send_dump(int s, const void *buf, size_t len, int flags)
{
	if(do_dump)
		_dump_packet("send", buf, len);
	return send(s, buf, len, flags);
}
#endif

void dump_packet(const char* buf, int buflen)
{
#ifdef ENABLE_DUMP
	_dump_packet("recv", buf, buflen);
#endif
}

void
print_packet( struct qserver *server, const char *buf, int buflen )
{
	output_packet( server, buf, buflen, 0 );
}

void
output_packet( struct qserver *server, const char *buf, int buflen, int to )
{
	static char *hex= "0123456789abcdef";
	unsigned char *p= (unsigned char*)buf;
	int i, h, a, b, astart, offset= 0;
	char line[256];

	if ( server != NULL)
	{
		fprintf( stderr, "%s %s len %d\n", ( to ) ? "TO" : "FROM", server->arg, buflen );
	}

	for ( i= buflen; i ; offset+= 16)
	{
		memset( line, ' ', 256);
		h= 0;
		h+= sprintf( line, "%5d:", offset);
		a= astart = h + 16*2 + 16/4 + 2;
		for ( b=16; b && i; b--, i--, p++)
		{
			if ( (b & 3) == 0)
			{
				line[h++]= ' ';
			}
			line[h++]= hex[*p >> 4];
			line[h++]= hex[*p & 0xf];
			if ( isprint( *p))
			{
				line[a++]= *p;
			}
			else
			{
				line[a++]= '.';
			}
			if((a-astart)==8)
			{
				line[a++] = ' ';
			}
		}
		line[a]= '\0';
		fputs( line, stderr);
		fputs( "\n", stderr);
	}
	fputs( "\n", stderr);
}

