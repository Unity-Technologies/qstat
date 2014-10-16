/*
 * qstat
 * by Steve Jankowski
 *
 * World in Conflict Protocol
 * Copyright 2007 Steven Hartland
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


query_status_t send_wic_request_packet( struct qserver *server )
{
	char buf[256];

	int serverport = get_param_i_value( server, "port", 0 );
	char *password = get_param_value( server, "password", "N/A" );
	change_server_port( server, serverport, 1 );

	if ( get_player_info )
	{
		server->flags |= TF_PLAYER_QUERY|TF_RULES_QUERY;
		sprintf( buf, "%s\x0d\x0a/listsettings\x0d\x0a/listplayers\x0d\x0a/exit\x0d\x0a", password );
		server->saved_data.pkt_index = 2;
	}
	else
	{
		server->flags |= TF_STATUS_QUERY;
		sprintf( buf, "%s\x0d\x0a/listsettings\x0d\x0a/exit\x0d\x0a", password );
		server->saved_data.pkt_index = 1;
	}

	return send_packet( server, buf, strlen( buf ) );
}


query_status_t deal_with_wic_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	char *s, *end, *team = NULL;
	int mode = server->n_servers, slot, score;
	char name[256], role[256];

	debug( 2, "processing n_requests %d, retry1 %d, n_retries %d, delta %d", server->n_requests, server->retry1, n_retries, time_delta( &packet_recv_time, &server->packet_time1 ) );

	server->ping_total += time_delta( &packet_recv_time, &server->packet_time1 );
	server->n_requests++;

	if ( 0 == pktlen )
	{
		// Invalid password
		return REQ_ERROR;
	}

	gettimeofday( &server->packet_time1, NULL);

	rawpkt[pktlen]= '\0';
	end = &rawpkt[pktlen];

	s = rawpkt;

	while ( NULL != s )
	{
		int len = strlen( s );
		*(s + len - 2) = '\0'; // strip off \x0D\x0A
		debug( 2, "Line[%d]: %s", mode, s );

		if ( 0 == mode )
		{
			// Settings
			// TODO: make parse safe
			if ( 0 == strncmp( s, "Settings: ", 9 ) )
			{
				// Server Rule
				char *key = s + 10;
				char *value = strchr( key, ' ' );
				*value = '\0';
				value++;
				debug( 2, "key: '%s' = '%s'", key, value );
				if ( 0 == strcmp( "myGameName", key ) )
				{
					server->server_name = strdup( value );
				}
				else if ( 0 == strcmp( "myMapFilename", key ) )
				{
					server->map_name = strdup( value );
				}
				else if ( 0 == strcmp( "myMaxPlayers", key ) )
				{
					server->max_players = atoi( value );
				}
				else if ( 0 == strcmp( "myCurrentNumberOfPlayers", key ) )
				{
					server->num_players = atoi( value);
				}
				else
				{
					add_rule( server, key, value, NO_FLAGS );
				}
			}
			else if ( 0 == strcmp( "Listing players", s ) )
			{
				// end of rules request
				server->saved_data.pkt_index--;
				mode++;
			}
			else if ( 0 == strcmp( "Exit confirmed.", s ) )
			{
				server->n_servers = mode;
				return DONE_FORCE;
			}
		}
		else if ( 1 == mode )
		{
			// Player info
			if ( 0 == strncmp( s, "Team: ", 6 ) )
			{
				team = s + 6;
				debug( 2, "Team: %s", team );
			}
			else if ( 4 == sscanf( s, "Slot: %d Role: %s Score: %d Name: %255[^\x0d\x0a]", &slot, role, &score, name ) )
			{
				// Player info
				struct player *player = add_player( server, server->n_player_info );
				if ( NULL != player )
				{
					player->flags |= PLAYER_FLAG_DO_NOT_FREE_TEAM;
					player->name = strdup( name );
					player->score = score;
					player->team_name = team;
					player->tribe_tag = strdup( role );
					// Indicate if its a bot
					player->type_flag = ( 0 == strcmp( name, "Computer: Balanced" ) ) ? 1 : 0;
				}
				debug( 2, "player %d, role %s, score %d, name %s", slot, role, score, name );
			}
			else if ( 3 == sscanf( s, "Slot: %d Role:  Score: %d Name: %255[^\x0d\x0a]", &slot, &score, name ) )
			{
				// Player info
				struct player *player = add_player( server, server->n_player_info );
				if ( NULL != player )
				{
					player->flags |= PLAYER_FLAG_DO_NOT_FREE_TEAM;
					player->name = strdup( name );
					player->score = score;
					player->team_name = team;
					// Indicate if its a bot
					player->type_flag = ( 0 == strcmp( name, "Computer: Balanced" ) ) ? 1 : 0;
				}
				debug( 2, "player %d, score %d, name %s", slot, score, name );
			}
			else if ( 0 == strcmp( "Exit confirmed.", s ) )
			{
				server->n_servers = mode;
				return DONE_FORCE;
			}
		}

		s += len;
		if ( s + 1 < end )
		{
			s++; // next line
		}
		else
		{
			s = NULL;
		}
	}

	server->n_servers = mode;

	if ( 0 == server->saved_data.pkt_index )
	{
		server->map_name = strdup( "N/A" );
		return DONE_FORCE;
	}

	return INPROGRESS;
}
