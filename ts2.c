/*
 * qstat
 * by Steve Jankowski
 *
 * Teamspeak 2 query protocol
 * Copyright 2005 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 *
 */

#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "debug.h"
#include "qstat.h"
#include "packet_manip.h"


query_status_t send_ts2_request_packet( struct qserver *server )
{
	char buf[256];

	int serverport = get_param_i_value( server, "port", 0 );
	change_server_port( server, serverport, 1 );

	if ( get_player_info )
	{
		server->flags |= TF_PLAYER_QUERY|TF_RULES_QUERY;
		sprintf( buf, "si %d\npl %d\nquit\n", serverport, serverport );
		server->saved_data.pkt_index = 2;
	}
	else
	{
		server->flags |= TF_STATUS_QUERY;
		sprintf( buf, "si %d\nquit\n", serverport );
		server->saved_data.pkt_index = 1;
	}

	return send_packet( server, buf, strlen( buf ) );
}


query_status_t deal_with_ts2_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	char *s;
	int ping, connect_time, mode = 0;
	char name[256];
	debug( 2, "processing..." );

	server->n_servers++;
	server->n_requests++;
	server->ping_total += time_delta( &packet_recv_time, &server->packet_time1 );

	if ( 0 == pktlen )
	{
		// Invalid password
		return REQ_ERROR;
	}

	rawpkt[pktlen]= '\0';

	s = rawpkt;
	s = strtok( rawpkt, "\015\012" );

	while ( NULL != s )
	{
		if ( 0 == mode )
		{
			// Rules
			char *key = s;
			char *value = strchr( key, '=' );
			if ( NULL != value )
			{
				// Server Rule
				*value = '\0';
				value++;
				if ( 0 == strcmp( "server_name", key ) )
				{
					server->server_name = strdup( value );
				}
				else if ( 0 == strcmp( "server_udpport", key ) )
				{
					change_server_port( server, atoi( value ), 0 );
					add_rule( server, key, value, NO_FLAGS );
				}
				else if ( 0 == strcmp( "server_maxusers", key ) )
				{
					server->max_players = atoi( value );
				}
				else if ( 0 == strcmp( "server_currentusers", key ) )
				{
					server->num_players = atoi( value);
				}
				else
				{
					add_rule( server, key, value, NO_FLAGS);
				}
			}
			else if ( 0 == strcmp( "OK", s ) )
			{
				// end of rules request
				server->saved_data.pkt_index--;
				mode++;
			}
			else if ( 0 == strcmp( "[TS]", s ) )
			{
				// nothing to do
			}
			else if ( 0 == strcmp( "ERROR, invalid id", s ) )
			{
				// bad server
				server->server_name = DOWN;
				server->saved_data.pkt_index = 0;
			}
		}
		else if ( 1 == mode )
		{
			// Player info
			if ( 3 == sscanf( s, "%*d %*d %*d %*d %*d %*d %*d %d %d %*d %*d %*d %*d \"0.0.0.0\" \"%255[^\"]", &ping, &connect_time, name ) )
			{
				// Player info
				struct player *player = add_player( server, server->n_player_info );
				if ( NULL != player )
				{
					player->name = strdup( name );
					player->ping = ping;
					player->connect_time = connect_time;
				}
			}
			else if ( 0 == strcmp( "OK", s ) )
			{
				// end of rules request
				server->saved_data.pkt_index--;
				mode++;
			}
			else if ( 0 == strcmp( "[TS]", s ) )
			{
				// nothing to do
			}
			else if ( 0 == strcmp( "ERROR, invalid id", s ) )
			{
				// bad server
				server->server_name = DOWN;
				server->saved_data.pkt_index = 0;
			}
		}
		s = strtok( NULL, "\015\012" );
	}

	gettimeofday( &server->packet_time1, NULL );

	if ( 0 == server->saved_data.pkt_index )
	{
		server->map_name = strdup( "N/A" );
		return DONE_FORCE;
	}

	return INPROGRESS;
}
