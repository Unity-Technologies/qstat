/*
 * qstat
 * by Steve Jankowski
 *
 * Terraria / TShock query protocol
 * Copyright 2012 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 *
 */

#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define strtok_ret strtok_r
#else
#include <winsock.h>
#define strtok_ret strtok_s
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "debug.h"
#include "qstat.h"
#include "packet_manip.h"

query_status_t send_terraria_request_packet( struct qserver *server )
{
    return send_packet( server, server->type->status_packet, server->type->status_len );
}

query_status_t deal_with_terraria_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	char *s, *key, *val, *sep, *linep, *varp;
	int complete_response = 0;
	char last_char;
	unsigned short port = 0;
	debug( 2, "processing..." );

    if ( 0 == pktlen )
    {
        // Disconnect?
        return REQ_ERROR;
    }

	complete_response = ( 0 == strncmp( "HTTP/1.1 200", rawpkt, 12 ) && '}' == rawpkt[pktlen-1] ) ? 1 : 0;
	last_char = rawpkt[pktlen-1];
	rawpkt[pktlen-1] = '\0';

	debug( 3, "packet: combined = %d, complete = %d", server->combined, complete_response );
	if ( ! complete_response )
	{
		if ( ! server->combined )
		{
			// response fragment recieved
			int pkt_id;
			int pkt_max;
			server->retry1 = n_retries;
			if ( 0 == server->n_requests )
			{
				server->ping_total = time_delta( &packet_recv_time, &server->packet_time1 );
				server->n_requests++;
			}

			// We're expecting more to come
			debug( 5, "fragment recieved..." );
			pkt_id = packet_count( server );
			pkt_max = pkt_id+1;
			rawpkt[pktlen-1] = last_char; // restore the last character
			if ( ! add_packet( server, 0, pkt_id, pkt_max, pktlen, rawpkt, 1 ) )
			{
				// fatal error e.g. out of memory
				return MEM_ERROR;
			}

			if ( 0 == pkt_id )
			{
				return INPROGRESS;
			}

			// combine_packets will call us recursively
			return combine_packets( server );
		}

		// recursive call which is still incomplete
		return INPROGRESS;
	}

	// find the end of the headers
	s = strstr( rawpkt, "\x0d\x0a\x0d\x0a" );

	if ( NULL == s )
	{
		debug(1, "Error: missing end of headers");
		return REQ_ERROR;
	}

	s += 4;

	// Correct ping
	// Not quite right but gives a good estimate
	server->ping_total = ( server->ping_total * server->n_requests ) / 2;

	debug( 3, "processing response..." );

	s = strtok_ret( s, "\x0d\x0a", &linep );

	// NOTE: id=XXX and msg=XXX will be processed by the mod following the one they where the response of
	while ( NULL != s )
	{
		debug( 2, "LINE: %s\n", s );
		if ( 0 == strcmp( s, "{" ) )
		{
			s = strtok_ret( NULL, "\x0d\x0a", &linep );
			continue;
		}

		s = strtok_ret( s, "\"", &varp );
		key = strtok_ret( NULL, "\"", &varp );
		sep = strtok_ret( NULL, " ", &varp );
		val = strtok_ret( NULL, "\"", &varp );
		if ( NULL == val )
		{
			// world etc may be empty which results in NULL val
			s = strtok_ret( NULL, "\x0d\x0a", &linep );
			continue;
		}
		//if ( NULL == val && sep
		debug( 2, "var: '%s' = '%s', sep: '%s'\n", key, val, sep );
		if ( 0 == strcmp( key, "name" ) )
		{
			server->server_name = strdup( val );
		}
		else if ( 0 == strcmp( key, "port" ) )
		{
			port = atoi( val );
			change_server_port( server, port, 0 );
			add_rule( server, key, val, NO_FLAGS );
		}
		else if ( 0 == strcmp( key, "playercount" ) )
		{
			server->num_players = atoi( val );
		}
		else if ( 0 == strcmp( key, "maxplayers" ) )
		{
			server->max_players = atoi( val );
		}
		else if ( 0 == strcmp( key, "world" ) )
		{
			server->map_name = strdup( val );
		}
		else if ( 0 == strcmp( key, "players" ) )
		{
			// Players are ", " seperated but potentially player names can have spaces
			// so we manually check for the leading space and fix if found
			char *playersp;
			char *player_name = strtok_ret( val, ",", &playersp );
			while ( NULL != player_name )
			{
				struct player *player = add_player( server, server->n_player_info );
				if ( NULL != player )
				{
					if ( ' ' == player_name[0] )
					{
						player_name++;
					}
					player->name = strdup( player_name );
					debug( 4, "Player: %s\n", player->name );
				}
				player_name = strtok_ret( NULL, ",", &playersp );
			}
		}
		else if ( 0 == strcmp( key, "status" ) )
		{
			if ( 200 != atoi( val ) )
			{
				server->server_name = DOWN;
				return DONE_FORCE;
			}
		}
		else
		{
			add_rule( server, key, val, NO_FLAGS);
		}

		s = strtok_ret( NULL, "\x0d\x0a", &linep );
	}

	gettimeofday( &server->packet_time1, NULL );

	return DONE_FORCE;
}

