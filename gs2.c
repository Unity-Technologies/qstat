/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * New Gamespy v2 query protocol
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

#include "debug.h"
#include "qstat.h"
#include "packet_manip.h"

void send_gs2_request_packet( struct qserver *server )
{
	int rc;

	// The below should work but seems to make no difference to what some
	// servers send
	if ( get_player_info )
	{
		server->type->status_packet[8] = 0xff;
		server->type->status_packet[9] = 0xff;
	}
	else
	{
		server->type->status_packet[8] = 0x00;
		server->type->status_packet[9] = 0x00;
	}

	if ( server->flags & FLAG_BROADCAST)
	{
		rc = send_broadcast( server, server->type->status_packet, server->type->status_len );
	}
	else
	{
		rc = send( server->fd, server->type->status_packet, server->type->status_len, 0 );
	}

	if ( rc == SOCKET_ERROR )
	{
		perror( "send" );
	}

	if ( server->retry1 == n_retries || server->flags & FLAG_BROADCAST)
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


// See the following for protocol details:
// http://dev.kquery.com/index.php?article=42
void deal_with_gs2_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	char *ptr = rawpkt;
	char *end = rawpkt + pktlen;
	unsigned char type = 0;
	unsigned char no_players = 0;
	unsigned char total_players = 0;
	unsigned char no_teams = 0;
	unsigned char total_teams = 0;
	unsigned char no_headers = 0;
	char **headers = NULL;
	debug( 2, "processing packet..." );

	if ( pktlen < 15 )
	{
		// invalid packet?
		cleanup_qserver( server, 1);
		return;
	}

	server->n_servers++;
	if ( server->server_name == NULL)
	{
		server->ping_total += time_delta( &packet_recv_time, &server->packet_time1);
	}
	else
	{
		gettimeofday( &server->packet_time1, NULL);
	}

	// Could check the header here should
	// match the 4 byte id sent
	ptr += 5;
	while ( 0 == type && ptr < end )
	{
		// server info:
		// name value pairs null seperated
		// empty name && value signifies the end of section
		char *var, *val;
		int var_len, val_len;

		var = ptr;
		var_len = strlen( var );

		if ( ptr + var_len + 2 > end )
		{
			if ( 0 != var_len )
			{
				malformed_packet( server, "no rule value" );
			}
			else if ( get_player_info )
			{
				malformed_packet( server, "no player headers" );
			}
			cleanup_qserver( server, 1);
			return;
		}
		ptr += var_len + 1;

		val = ptr;
		val_len = strlen( val );
		ptr += val_len + 1;
		debug( 2, "var:%s (%d)=%s (%d)\n", var, var_len, val, val_len );

		// Lets see what we've got
		if ( 0 == strcmp( var, "hostname" ) )
		{
			server->server_name = strdup( val );
		}
		else if( 0 == strcmp( var, "game_id" ) )
		{
			server->game = strdup( val );
		}
		else if( 0 == strcmp( var, "gamever" ) )
		{
			// format:
			// v1.0
			server->protocol_version = atoi( val+1 );
			add_rule( server, var, val, NO_FLAGS );
		}
		else if( 0 == strcmp( var, "mapname" ) )
		{
			server->map_name = strdup( val );
		}
		else if( 0 == strcmp( var, "maxplayers" ) )
		{
			server->max_players = atoi( val );

		}
		else if( 0 == strcmp( var, "numplayers" ) )
		{
			server->num_players = no_players = atoi( val );
		}
		else if( 0 == strcmp( var, "hostport" ) )
		{
			change_server_port( server, atoi( val ), 1 );
		}
		else if ( 0 == var_len )
		{
			// check for end of section
			type = 1;
		}
		else
		{
			add_rule( server, var, val, NO_FLAGS );
		}
	}

	if ( 1 != type )
	{
		// no more info should be player headers here as we
		// requested it
		malformed_packet( server, "no player headers" );
		cleanup_qserver( server, 1);
		return;
	}

	// player info header
	// format:
	// first byte = player count
	// followed by null seperated header
	no_players = (unsigned char)*ptr;
	debug( 2, "No Players:%d\n", no_players );
	ptr++;

	if ( ptr >= end )
	{
		malformed_packet( server, "no player headers" );
		cleanup_qserver( server, 1);
		return;
	}

	while ( 1 == type && ptr < end )
	{
		// first we have the headers null seperated
		char **tmpp;
		char *head = ptr;
		int head_len = strlen( head );
		no_headers++;
		tmpp = (char**)realloc( headers, no_headers * sizeof( char* ) );
		if ( NULL == tmpp )
		{
			debug( 0, "Failed to realloc memory for headers\n" );
			if ( NULL != headers )
			{
				free( headers );
			}
			cleanup_qserver( server, 1);
			return;
		}

		headers = tmpp;
		headers[no_headers-1] = head;

		ptr += head_len + 1;

		// end of headers check
		if ( 0x00 == *ptr )
		{
			type = 2;
			ptr++;
		}
		debug( 2, "player header[%d] = '%s'", no_headers-1, head );
	}

	if ( 2 != type )
	{
		// no more info should be player info here as we
		// requested it
		malformed_packet( server, "no players" );
		cleanup_qserver( server, 1);
		return;
	}

	while( 2 == type && ptr < end )
	{
		// now each player details
		// add the player
		if ( 0x00 == *ptr )
		{
			// no players
			if ( 0 != no_players )
			{
				malformed_packet( server, "no players" );
				cleanup_qserver( server, 1);
				return;
			}
		}
		else
		{
			struct player *player = add_player( server, total_players );
			int i;
			for ( i = 0; i < no_headers; i++ )
			{
				char *header = headers[i];
				char *val;
				int val_len;

				if ( ptr >= end )
				{
					malformed_packet( server, "short player detail" );
					cleanup_qserver( server, 1);
					return;
				}
				val = ptr;
				val_len = strlen( val );
				ptr += val_len + 1;

				// lets see what we got
				if ( 0 == strcmp( header, "player_" ) )
				{
					player->name = strdup( val );
				}
				else if ( 0 == strcmp( header, "score_" ) )
				{
					player->score = atoi( val );
				}
				else if ( 0 == strcmp( header, "deaths_" ) )
				{
					player->deaths = atoi( val );
				}
				else if ( 0 == strcmp( header, "ping_" ) )
				{
					player->ping = atoi( val );
				}
				else if ( 0 == strcmp( header, "kills_" ) )
				{
					player->frags = atoi( val );
				}
				else if ( 0 == strcmp( header, "team_" ) )
				{
					player->team = atoi( val );
				}
				else
				{
					int len = strlen( header );
					if ( '_' == header[len-1] )
					{
						header[len-1] = '\0';
					}
					player_add_info( player, header, val, NO_FLAGS );
				}

				debug( 2, "Player[%d][%s]=%s\n", total_players, headers[i], val );
			}
			total_players++;
		}

		if ( total_players > no_players )
		{
			malformed_packet( server, "to many players %d > %d", total_players, no_players );
			cleanup_qserver( server, 1);
			return;
		}

		// check for end of player info
		if ( 0x00 == *ptr )
		{
			if ( total_players != no_players )
			{
				malformed_packet( server, "bad number of players %d != %d", total_players, no_players );
				cleanup_qserver( server, 1);
				return;
			}
			type = 3;
			ptr++;
		}
	}

	if ( 3 != type )
	{
		// no more info should be team info here as we
		// requested it
		malformed_packet( server, "no teams" );
		cleanup_qserver( server, 1 );
		return;
	}

	no_teams = (unsigned char)*ptr;
	ptr++;

	debug( 2, "No teams:%d\n", no_teams );
	no_headers = 0;

	while ( 3 == type && ptr < end )
	{
		// first we have the headers null seperated
		char **tmpp;
		char *head = ptr;
		int head_len = strlen( head );
		no_headers++;
		tmpp = (char**)realloc( headers, no_headers * sizeof( char* ) );
		if ( NULL == tmpp )
		{
			debug( 0, "Failed to realloc memory for headers\n" );
			if ( NULL != headers )
			{
				free( headers );
			}
			cleanup_qserver( server, 1);
			return;
		}

		headers = tmpp;
		headers[no_headers-1] = head;

		ptr += head_len + 1;

		// end of headers check
		if ( 0x00 == *ptr )
		{
			type = 4;
			ptr++;
		}
	}

	if ( 4 != type )
	{
		// no more info should be team info here as we
		// requested it
		malformed_packet( server, "no teams" );
		cleanup_qserver( server, 1);
		return;
	}

	while( 4 == type && ptr < end )
	{
		// now each teams details
		int i;
		for ( i = 0; i < no_headers; i++ )
		{
			char *val;
			int val_len;

			if ( ptr >= end )
			{
				malformed_packet( server, "short team detail" );
				cleanup_qserver( server, 1);
				return;
			}
			val = ptr;
			val_len = strlen( val );
			ptr += val_len + 1;

			// lets see what we got
			if ( 0 == strcmp( headers[i], "team_t" ) )
			{
				// BF being stupid again teams 1 based instead of 0
				players_set_teamname( server, total_teams + 1, val );
			}
			debug( 2, "Team[%d][%s]=%s\n", total_teams, headers[i], val );
		}
		total_teams++;

		if ( total_teams > no_teams )
		{
			malformed_packet( server, "to many teams" );
			cleanup_qserver( server, 1);
			return;
		}
	}

	cleanup_qserver( server, 1);
	return;
}
