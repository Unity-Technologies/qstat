/*
 * qstat 2.6
 * by Steve Jankowski
 *
 * debug helper functions
 * Copyright 2004 Ludwig Nussel
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */

#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/param.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <stdarg.h>

#ifdef DEBUG
void _debug(const char* file, int line, const char* function, int level, const char* fmt, ...)
{
	va_list ap;
	char buf[9];
	time_t now;

	now = time(NULL);
	strftime(buf,9,"%T",localtime(&now));

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

void malformed_packet(struct qserver* server, const char* fmt, ...)
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
#endif
void dump_packet(const char* buf, int buflen)
{
#ifdef ENABLE_DUMP
	char fn[PATH_MAX] = {0};
	int fd;
	sprintf(fn, "dump%03u", count++);
	fprintf(stderr, "dumping to %s\n", fn);
	fd = open(fn, O_WRONLY|O_CREAT|O_EXCL, 0644);
	if(fd == -1) { perror("open"); return; }
	if(write(fd, buf, buflen) == -1)
		perror("write");
	close(fd);
#endif
}

void
print_packet( struct qserver *server, char *buf, int buflen)
{
	static char *hex= "0123456789abcdef";
	unsigned char *p= (unsigned char*)buf;
	int i, h, a, b, astart, offset= 0;
	char line[256];

	if ( server != NULL)
	fprintf( stderr, "FROM %s len %d\n", server->arg, buflen );

	for ( i= buflen; i ; offset+= 16)  {
	memset( line, ' ', 256);
	h= 0;
	h+= sprintf( line, "%5d:", offset);
	a= astart = h + 16*2 + 16/4 + 2;
	for ( b=16; b && i; b--, i--, p++)  {
		if ( (b & 3) == 0)
		line[h++]= ' ';
		line[h++]= hex[*p >> 4];
		line[h++]= hex[*p & 0xf];
		if ( isprint( *p))
		line[a++]= *p;
		else
		line[a++]= '.';
		if((a-astart)==8) line[a++] = ' ';
	}
	line[a]= '\0';
	fputs( line, stderr);
	fputs( "\n", stderr);
	}
	fputs( "\n", stderr);
}

