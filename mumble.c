/*
 * qstat 2.11
 * by Steve Jankowski
 *
 * Mumble protocol
 * Copyright 2012 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 *
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#ifndef _WIN32
#include <arpa/inet.h>
#endif

#include "debug.h"
#include "qstat.h"
#include "packet_manip.h"

query_status_t send_mumble_request_packet( struct qserver *server )
{
	return send_packet( server, server->type->status_packet, server->type->status_len );
}

query_status_t deal_with_mumble_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	// skip unimplemented ack, crc, etc
	char *pkt = rawpkt;
	char bandwidth[11];
	char version[11];

	server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
	if ( 24 != pktlen || 0 != memcmp( pkt+4, server->type->status_packet+4, 8 ) )
	{
		// unknown packet
		return PKT_ERROR;
	}

	// version
	server->protocol_version = ntohl(*(unsigned long*)pkt);
	sprintf(version,"%u.%u.%u", (unsigned char)*pkt+1, (unsigned char)*pkt+2, (unsigned char)*pkt+3);
	add_rule( server, "version", version, NO_FLAGS );
	pkt += 4;

	// ident
	pkt += 8;

	// num players
	server->num_players = ntohl(*(unsigned long*)pkt);
	pkt += 4;

	// max players
	server->max_players = ntohl(*(unsigned long*)pkt);
	pkt += 4;

	// allowed bandwidth
	sprintf( bandwidth, "%d", ntohl(*(unsigned long*)pkt));
	add_rule( server, "allowed_bandwidth", bandwidth, NO_FLAGS );
	pkt += 4;

	// Unknown details
	server->map_name = strdup( "N/A" );
	server->server_name = strdup( "Unknown" );
	add_rule( server, "gametype", "Unknown", NO_FLAGS );

	return DONE_FORCE;
}
