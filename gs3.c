/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * New Gamespy v3 query protocol
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

int process_gs3_packet( struct qserver *server );

// Player headers
#define PLAYER_NAME_HEADER 1
#define PLAYER_SCORE_HEADER 2
#define PLAYER_DEATHS_HEADER 3
#define PLAYER_PING_HEADER 4
#define PLAYER_KILLS_HEADER 5
#define PLAYER_TEAM_HEADER 6
#define PLAYER_OTHER_HEADER 7

// Team headers
#define TEAM_NAME_HEADER 1
#define TEAM_OTHER_HEADER 2

void deal_with_gs3_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	char *ptr = rawpkt;
	unsigned int pkt_id;
	int pkt_index;
	unsigned char flag;
	unsigned int pkti, final;

	debug( 2, "packet..." );

	if ( pktlen < 12 )
	{
		// invalid packet?
		fprintf( stderr, "Invalid packet ( too short )\n" );
		cleanup_qserver( server, 1 );
		return;
	}

	if ( 0x00 != *ptr )
	{
		fprintf( stderr, "Invalid packet ( bad initial byte '%hhx' )\n", *ptr );
		cleanup_qserver( server, 1 );
		return;
	}
	ptr++;

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
	memcpy( &pkt_id, ptr, 4 );
	ptr += 4;

	// Next we have the splitnum details
	if ( 0 != strncmp( ptr, "splitnum", 8 ) )
	{
		if ( server->flags & TF_STATUS_QUERY )
		{
			// we have the status response
			deal_with_gs3_status( server, ptr, pktlen - ( ptr - rawpkt )  );
			return;
		}
		else
		{
			fprintf( stderr, "Invalid packet ( missing splitnum )\n" );
			cleanup_qserver( server, 1 );
			return;
		}
	}
	ptr += 9;

	pkt_index = ((unsigned char)*ptr) & 127;
	final = ((unsigned char)*ptr) >> 7;
	flag = *ptr++;
	pkti = *ptr++;

	debug( 1, "splitnum: flag = 0x%hhx, index = %d, final = %d, %d", flag, pkt_index, final, pkti );
	if ( 0xFF != flag )
	{
		// not a single packet response or a callback
		int pkt_max = pkt_index + 1;

		if ( ! final )
		{
			// Guess that we have more to come
			pkt_max++;
			debug( 2, "more to come 0x%hxx 0x%hhx 0x%hhx", rawpkt[pktlen-3], rawpkt[pktlen-2], rawpkt[pktlen-1] );
		}

		debug( 2, "pkt_max %d", pkt_max );

		if ( 0 == pkt_index )
		{
			// to prevent reprocessing when we get the call back
			// override the packet flag so it looks like a single
			// packet response
			rawpkt[14] = 0xFF;
		}

		// add the packet recalcing maxes
		if ( ! add_packet( server, pkt_id, pkt_index, pkt_max, pktlen, rawpkt, 1 ) )
		{
			// fatal error e.g. out of memory
			return;
		}

		// combine_packets will call us recursively
		combine_packets( server );
		return;
	}

	// if we get here we have what should be a full packet
	process_gs3_packet( server );
	return;
}

void deal_with_gs3_status( struct qserver *server, char *rawpkt, int pktlen )
{
	unsigned char *pkt = rawpkt;
	unsigned short port;
	debug( 1, "status packet" );

	// Server name
	server->server_name = strdup( pkt );
	pkt += strlen( pkt ) + 1;

	// gametype
	add_rule( server, "gametype", pkt, NO_FLAGS );
	pkt += strlen( pkt ) + 1;

	// map
	server->map_name = strdup( pkt );
	pkt += strlen( pkt ) + 1;

	// num players
	server->num_players = atoi( pkt );
	pkt += strlen( pkt ) + 1;

	// max_players
	server->max_players = atoi( pkt );
	pkt += strlen( pkt ) + 1;

	// hostport
	port = atoi( pkt );
	if ( port != server->port )
	{
		if ( show_game_port || server->flags & TF_SHOW_GAME_PORT )
		{
			change_server_port( server, port );
		}
		else
		{
			// N.B. Pointless really as we weren't asked for rules
			add_rule( server, "hostport", pkt, NO_FLAGS);
		}
    }
	pkt += strlen( pkt ) + 1;

	cleanup_qserver( server, 1 );
}

int process_gs3_packet( struct qserver *server )
{
	unsigned char state = 0;
	unsigned char no_players = 0;
	unsigned char total_players = 0;
	unsigned char no_teams = 0;
	unsigned char total_teams = 0;
	int pkt_index = 0;
	SavedData *fragment;

	debug( 2, "processing packet..." );

	while ( NULL != ( fragment = get_packet_fragment( pkt_index++ ) ) )
	{
		int pktlen = fragment->datalen;
		char *ptr = fragment->data;
		char *end = ptr + pktlen;
		debug( 2, "processing fragment[%d]...", fragment->pkt_index );

		// check we have a full header
		if ( pktlen < 16 )
		{
			// invalid packet?
			fprintf( stderr, "Invalid packet ( too short )\n" );
			cleanup_qserver( server, 1 );
			return 0;
		}

		// skip over the header
		ptr += 16;

		while ( 0 == state && ptr < end )
		{
			// server info:
			// name value pairs null seperated
			// empty name && value signifies the end of section
			char *var, *val;
			int var_len, val_len;

			if ( 0x00 == ptr[0] && 0x01 == ptr[1] )
			{
				// not quite sure of the significance of these bytes
				// but we use them as a check for end of section
				state = 1;
				ptr += 2;
				break;
			}

			var = ptr;
			var_len = strlen( var );
			ptr += var_len + 1;

			if ( ptr + 1 > end )
			{
				fprintf( stderr, "Invalid packet detected (no rule value)\n" );
				cleanup_qserver( server, 1);
				return 0;
			}

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

			else
			{
				add_rule( server, var, val, NO_FLAGS );
			}
		}

		while ( 1 == state && ptr < end )
		{
			// first we have the header
			char *header = ptr;
			int head_len = strlen( header );
			int header_type;
			ptr += head_len + 1;

			if ( 0 == head_len )
			{
				// no more info
				debug( 3, "All done" );
				cleanup_qserver( server, 1 );
				return 1;
			}

			debug( 2, "player header '%s'", header );

			if ( ptr > end )
			{
				fprintf( stderr, "Invalid packet detected (no details for header '%s')", header );
				cleanup_qserver( server, 1 );
				return 0;
			}

			// the next byte is the starting number
			total_players = *ptr++;

			if ( 0 == strcmp( header, "player_" ) )
			{
				header_type = PLAYER_NAME_HEADER;
			}
			else if ( 0 == strcmp( header, "score_" ) )
			{
				header_type = PLAYER_SCORE_HEADER;
			}
			else if ( 0 == strcmp( header, "deaths_" ) )
			{
				header_type = PLAYER_DEATHS_HEADER;
			}
			else if ( 0 == strcmp( header, "ping_" ) )
			{
				header_type = PLAYER_PING_HEADER;
			}
			else if ( 0 == strcmp( header, "kills_" ) )
			{
				header_type = PLAYER_KILLS_HEADER;
			}
			else if ( 0 == strcmp( header, "team_" ) )
			{
				header_type = PLAYER_TEAM_HEADER;
			}
			else
			{
				header_type = PLAYER_OTHER_HEADER;
			}

			while( ptr < end )
			{
				// now each player details
				// add the player
				struct player *player;
				char *val;
				int val_len;

				// check for end of this headers player info
				if ( 0x00 == *ptr )
				{
					debug( 3, "end of '%s' detail", header );
					ptr++;
					// Note: can't check ( total_players != no_players ) here as we may have more packets
					if ( ptr < end && 0x00 == *ptr )
					{
						debug( 3, "end of players" );
						// end of all player headers / detail
						state = 2;
						ptr++;
					}
					break;
				}

				player = get_player_by_number( server, total_players );
				if ( NULL == player )
				{
					player = add_player( server, total_players );
				}

				if ( ptr >= end )
				{
					fprintf( stderr, "Invalid packet detected (short player detail)\n" );
					cleanup_qserver( server, 1);
					return 0;
				}
				val = ptr;
				val_len = strlen( val );
				ptr += val_len + 1;

				debug( 2, "Player[%d][%s]=%s\n", total_players, header, val );

				// lets see what we got
				switch( header_type )
				{
				case PLAYER_NAME_HEADER:
					player->name = strdup( val );
					break;

				case PLAYER_SCORE_HEADER:
					player->score = atoi( val );
					break;

				case PLAYER_DEATHS_HEADER:
					player->deaths = atoi( val );
					break;

				case PLAYER_PING_HEADER:
					player->ping = atoi( val );
					break;

				case PLAYER_KILLS_HEADER:
					player->frags = atoi( val );
					break;

				case PLAYER_TEAM_HEADER:
					player->team = atoi( val );
					break;

				case PLAYER_OTHER_HEADER:
				default:
					if ( '_' == header[head_len-1] )
					{
						header[head_len-1] = '\0';
						player_add_info( player, header, val, NO_FLAGS );
						header[head_len-1] = '_';
					}
					else
					{
						player_add_info( player, header, val, NO_FLAGS );
					}
					break;
				}

				total_players++;

				if ( total_players > no_players )
				{
					fprintf( stderr, "Invalid packet detected (to many players %d > %d)\n", total_players, no_players );
					cleanup_qserver( server, 1 );
					return 0;
				}
			}
		}

		if ( 2 == state )
		{
			no_teams = (unsigned char)*ptr;
			ptr++;

			debug( 2, "No teams:%d\n", no_teams );
			state = 3;
		}

		while ( 3 == state && ptr < end )
		{
			// first we have the header
			char *header = ptr;
			int head_len = strlen( header );
			int header_type;
			ptr += head_len + 1;

			if ( 0 == head_len )
			{
				// no more info
				debug( 3, "All done" );
				cleanup_qserver( server, 1 );
				return 1;
			}

			debug( 2, "team header '%s'", header );
			if ( 0 == strcmp( header, "team_t" ) )
			{
				header_type = TEAM_NAME_HEADER;
			}
			else
			{
				header_type = TEAM_OTHER_HEADER;
			}

			// the next byte is the starting number
			total_teams = *ptr++;

			while( ptr < end )
			{
				// now each teams details
				char *val;
				int val_len;
				char rule[512];

				if ( ptr >= end )
				{
					fprintf( stderr, "Invalid packet detected (short team detail)\n" );
					cleanup_qserver( server, 1);
					return 0;
				}
				val = ptr;
				val_len = strlen( val );
				ptr += val_len + 1;

				debug( 2, "Team[%d][%s]=%s\n", total_teams, header, val );

				// lets see what we got
				switch ( header_type )
				{
				case TEAM_NAME_HEADER:
					// BF being stupid again teams 1 based instead of 0
					players_set_teamname( server, total_teams + 1, val );
					// N.B. yes no break

				case TEAM_OTHER_HEADER:
				default:
					// add as a server rule
					sprintf( rule, "%s%d", header, total_teams );
					add_rule( server, rule, val, NO_FLAGS );
					break;
				}

				total_teams++;
				if ( 0x00 == *ptr )
				{
					// end of this headers teams
					ptr++;
					break;
				}
			}
		}
	}

	cleanup_qserver( server, 1 );
	return 1;
}

void send_gs3_request_packet( struct qserver *server )
{
	int rc;
	unsigned char *packet;
	int len;

	// The below should work but seems to make no difference to what some
	// servers send
	if ( get_player_info || get_server_rules )
	{
		server->flags |= TF_PLAYER_QUERY|TF_RULES_QUERY;
		packet = server->type->player_packet;
		len = server->type->player_len;
	}
	else
	{
		server->flags |= TF_STATUS_QUERY;
		packet = server->type->status_packet;
		len = server->type->status_len;
	}

	if ( server->flags & FLAG_BROADCAST)
	{
		rc = send_broadcast( server, packet, len );
	}
	else
	{
		rc = send( server->fd, packet, len, 0 );
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