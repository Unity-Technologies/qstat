/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * Gamespy query protocol
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


void send_ts2_request_packet( struct qserver *server )
{
	int rc;
	char buf[256];

	int serverport = get_param_i_value( server, "port", 0 );
	change_server_port( server, serverport );

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
	debug( 2, "[TS2] send '%s'", buf );
	if ( server->flags & FLAG_BROADCAST)
	{
		rc = send_broadcast( server, buf, strlen( buf ) );
	}
	else
	{
		rc = send( server->fd, buf, strlen( buf ), 0 );
	}

	if ( rc == SOCKET_ERROR)
	{
		perror( "send" );
	}

	if ( server->retry1 == n_retries || server->flags & FLAG_BROADCAST )
	{
		gettimeofday( &server->packet_time1, NULL);
		server->n_requests++;
	}
	else
	{
		server->n_retries++;
	}
	server->retry1--;
	server->n_packets++;
}


void deal_with_ts2_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	char *s, *key, *value, *end;
	struct player *player = NULL;
	int id_major=0, id_minor=0, final=0, player_num;
	char tmp[256];
	int total_players = 0;
	int ping, connect_time;
	char name[256];
	debug( 2, "processing..." );

	server->n_servers++;
	if ( server->server_name == NULL)
	{
		server->ping_total += time_delta( &packet_recv_time, &server->packet_time1 );
	}
	else
	{
		gettimeofday( &server->packet_time1, NULL);
	}

	rawpkt[pktlen]= '\0';
	end = &rawpkt[pktlen];

	s = rawpkt;
	s = strtok( rawpkt, "\015\012" );

	while ( NULL != s )
	{
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
				unsigned short port = atoi( value );
				change_server_port( server, port );
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
		else if ( 3 == sscanf( s, "%*d %*d %*d %*d %*d %*d %*d %d %d %*d %*d %*d %*d \"0.0.0.0\" \"%[^\"]", &ping, &connect_time, name ) )
		{
			// Player info
			struct player *player = add_player( server, total_players++ );
			player->name = strdup( name );
			player->ping = ping;
			player->connect_time = connect_time;
		}
		else if ( 0 == strcmp( "OK", s ) )
		{
			// end of request result
			server->saved_data.pkt_index--;
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
		s = strtok( NULL, "\015\012" );
	}

	if ( 0 == server->saved_data.pkt_index )
	{
		server->map_name = strdup( "N/A" );
		cleanup_qserver( server, 1 );
	}
}
