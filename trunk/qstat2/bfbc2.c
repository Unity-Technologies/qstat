/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * Battlefield Bad Company 2 query protocol
 * Copyright 2009 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 *
 */

#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "debug.h"
#include "qstat.h"
#include "packet_manip.h"

query_status_t send_bfbc2_request_packet( struct qserver *server )
{
	char buf[50];
	int size;
	switch ( server->challenge )
	{
	case 0:
		// Initial connect send serverInfo
		size = 27;
		memcpy( buf, "\x00\x00\x00\x00\x1b\x00\x00\x00\x01\x00\x00\x00\x0a\x00\x00\x00serverInfo\x00", size );
		break;

	case 1:
		// All Done send quit
		size = 21;
		memcpy( buf, "\x01\x00\x00\x00\x15\x00\x00\x00\x01\x00\x00\x00\x04\x00\x00\x00quit\x00", size );
		break;

	case 2:
		return DONE_FORCE;
	}
	debug( 3, "send_bfbc2_request_packet: state = %ld", server->challenge );

	return send_packet( server, buf, size );
}

query_status_t deal_with_bfbc2_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	char *s, *end;
	int size, words, word = 0;
	debug( 2, "processing..." );

	if ( 17 > pktlen )
	{
		// Invalid packet
		return REQ_ERROR;
	}

	rawpkt[pktlen-1] = '\0';
	end = &rawpkt[pktlen-1];
	s = rawpkt;

	server->ping_total = time_delta( &packet_recv_time, &server->packet_time1 );
	server->n_requests++;

	// Header Sequence
	s += 4;

	// Packet Size
	size = *(int*)s;
	s += 4;

	// Num Words
	words = *(int*)s;
	s += 4;

	// Words
	while ( words > 0 && s + 5 < end )
	{
		// Size
		int ws = *(int*)s;
		s += 4;

		// Content
		debug( 6, "word: %s\n", s );
		switch ( word )
		{
		case 0:
			// Status
			break;
		case 1:
			// Server Name
			server->server_name = strdup( s );
			break;
		case 2:
			// Player Count
			server->num_players = atoi( s );
			break;
		case 3:
			// Max Players
			server->max_players = atoi( s );
			break;
		case 4:
			// Game Mode
			add_rule( server, "gametype", s, NO_FLAGS );
			break;
		case 5:
			// Map
			server->map_name = strdup( s );
			break;
		}
		word++;

		s += ws + 1;

		words--;
	}

	server->challenge++;
	gettimeofday( &server->packet_time1, NULL );

	if ( 1 == server->challenge )
	{
		send_bfbc2_request_packet( server );
		return INPROGRESS;
	}

	return DONE_FORCE;
}

