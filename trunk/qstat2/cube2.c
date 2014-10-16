/*
 * qstat
 * by Steve Jankowski
 *
 * Cube 2 / Sauerbraten protocol
 * Copyright 2011 NoisyB
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "debug.h"
#include "qstat.h"
#include "packet_manip.h"

struct offset
{
	unsigned char *d;
	int pos;
	int len;
};


//#define SB_MASTER_SERVER "http://sauerbraten.org/masterserver/retrieve.do?item=list"
#define SB_PROTOCOL 258
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX_ATTR 255
#define MAX_STRING 1024


static int
getint (struct offset * d)
{
	int val = 0;

	if ( d->pos >= d->len )
	{
		return 0;
	}

	val = d->d[d->pos++] & 0xff; // 8 bit value

	// except...
	if ( val == 0x80 && d->pos < d->len - 2 ) // 16 bit value
	{
		val = (d->d[d->pos++] & 0xff);
		val |= (d->d[d->pos++] & 0xff) << 8;
	}
	else if ( val == 0x81 && d->pos < d->len - 4 ) // 32 bit value
	{
		val = (d->d[d->pos++] & 0xff);
		val |= (d->d[d->pos++] & 0xff) << 8;
		val |= (d->d[d->pos++] & 0xff) << 16;
		val |= (d->d[d->pos++] & 0xff) << 24;
	}

	return val;
}


static char * getstr( char *dest, int dest_len, struct offset *d )
{
	int len = 0;

	if (d->pos >= d->len)
	{
		return NULL;
	}

	len = MIN( dest_len, d->len - d->pos );
	strncpy( dest, (const char *) d->d + d->pos, len )[len - 1] = 0;
	d->pos += strlen (dest) + 1;

	return dest;
}


static char* sb_getversion_s (int n)
{
	static char *version_s[] =
	{
		"Justice",
		"CTF",
		"Assassin",
		"Summer",
		"Spring",
		"Gui",
		"Water",
		"Normalmap",
		"Sp",
		"Occlusion",
		"Shader",
		"Physics",
		"Mp",
		"",
		"Agc",
		"Quakecon",
		"Independence"
	};

	n = SB_PROTOCOL - n;
	if (n >= 0 && (size_t) n < sizeof(version_s) / sizeof(version_s[0]))
	{
		return version_s[n];
	}
	return "unknown";
}


static char* sb_getmode_s(int n)
{
	static char *mode_s[] =
	{
		"slowmo SP",
		"slowmo DMSP",
		"demo",
		"SP",
		"DMSP",
		"ffa/default",
		"coopedit",
		"ffa/duel",
		"teamplay",
		"instagib",
		"instagib team",
		"efficiency",
		"efficiency team",
		"insta arena",
		"insta clan arena",
		"tactics arena",
		"tactics clan arena",
		"capture",
		"insta capture",
		"regen capture",
		"assassin",
		"insta assassin",
		"ctf",
		"insta ctf"
	};

	n += 6;
	if (n >= 0 && (size_t) n < sizeof(mode_s) / sizeof(mode_s[0]))
	{
		return mode_s[n];
	}
	return "unknown";
}


query_status_t send_cube2_request_packet( struct qserver *server )
{
	return send_packet( server, server->type->status_packet, server->type->status_len );
}


query_status_t deal_with_cube2_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	// skip unimplemented ack, crc, etc
	int i;
	int numattr;
	int attr[MAX_ATTR];
	char buf[MAX_STRING];
	enum {
		MM_OPEN = 0,
		MM_VETO,
		MM_LOCKED,
		MM_PRIVATE
	};
	struct offset d;
	d.d = (unsigned char *) rawpkt;
	d.pos = 0;
	d.len = pktlen;

	server->ping_total += time_delta( &packet_recv_time, &server->packet_time1 );
	getint( &d ); // we have the ping already
	server->num_players = getint( &d );
	numattr = getint( &d );
	for ( i = 0; i < numattr && i < MAX_ATTR; i++ )
	{
		attr[i] = getint (&d);
	}

	server->protocol_version = attr[0];

	sprintf( buf, "%d %s", attr[0], sb_getversion_s (attr[0]) );
	add_rule( server, "version", buf, NO_FLAGS );

	sprintf( buf, "%d %s", attr[1], sb_getmode_s (attr[1]) );
	add_rule( server, "mode", buf, NO_FLAGS );

	sprintf( buf, "%d", attr[2] );
	add_rule( server, "seconds_left", buf, NO_FLAGS );

	server->max_players = attr[3];

	switch ( attr[5] )
	{
	case MM_OPEN:
		sprintf( buf, "%d open", attr[5] );
		break;
	case MM_VETO:
		sprintf( buf, "%d veto", attr[5] );
		break;
	case MM_LOCKED:
		sprintf( buf, "%d locked", attr[5] );
		break;
	case MM_PRIVATE:
		sprintf( buf, "%d private", attr[5] );
		break;
	default:
		sprintf( buf, "%d unknown", attr[5] );
	}
	add_rule( server, "mm", buf, NO_FLAGS);

	for ( i = 0; i < numattr && i < MAX_ATTR; i++ )
	{
		char buf2[MAX_STRING];
		sprintf( buf, "attr%d", i );
		sprintf( buf2, "%d", attr[i] );
		add_rule( server, buf, buf2, NO_FLAGS );
	}

	getstr( buf, MAX_STRING, &d );
	server->map_name = strdup(buf);
	getstr( buf, MAX_STRING, &d );
	server->server_name = strdup(buf);

	return DONE_FORCE;
}


