/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * New Haze query protocol
 * Copyright 2005 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 *
 */

#include <sys/types.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#include "debug.h"
#include "qstat.h"
#include "packet_manip.h"


// Format:
// 1 - 8: Challenge Request / Response
unsigned char haze_challenge[] = {
    'f', 'r', 'd', 'c', '_', '_', '_', '_'
};

int process_haze_packet( struct qserver *server );

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

// Challenge response algorithum
// Before sending a qr2 query (type 0x00) the client must first send a
// challenge request (type 0x09).  The host will respond with the same
// packet type containing a string signed integer.
//
// Once the challenge is received the client should convert the string to a
// network byte order integer and embed it in the keys query.
//
// Example:
//
// 	challenge request: [0xFE][0xFD][0x09][0x.. 4-byte-instance]
//	challenge response: [0x09][0x.. 4-byte-instance]["-1287574694"]
//	query: [0xFE][0xFD][0x00][0x.. 4-byte-instance][0xb3412b5a "-1287574694"]
//

void deal_with_haze_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	char *ptr = rawpkt;
	unsigned int pkt_id;
	unsigned short len;
	unsigned char pkt_max, pkt_index;

	debug( 2, "packet..." );

	if ( pktlen < 8 )
	{
		// invalid packet
		malformed_packet( server, "too short" );
		cleanup_qserver( server, 1 );
		return;
	}

	if ( 0 == strncmp( ptr, "frdcr", 5 ) )
	{
		// challenge response
		ptr += 8;
		server->challenge = 1;

		// Correct the stats due to two phase protocol
		server->retry1++;
		server->n_packets--;
		if ( server->retry1 == n_retries || server->flags & FLAG_BROADCAST )
		{
			server->n_requests--;
		}
		else
		{
			server->n_retries--;
		}
		send_haze_request_packet( server );
		return;
	}

	if ( pktlen < 12 )
	{
		// invalid packet
		malformed_packet( server, "too short" );
		cleanup_qserver( server, 1 );
		return;
	}

	server->n_servers++;
	if ( server->server_name == NULL )
	{
		server->ping_total += time_delta( &packet_recv_time, &server->packet_time1 );
	}
	else
	{
		gettimeofday( &server->packet_time1, NULL);
	}

	// Query version ID
	ptr += 4;

	// Could check the header here should
	// match the 4 byte id sent
	memcpy( &pkt_id, ptr, 4 );
	ptr += 4;


	// Max plackets
	pkt_max = ((unsigned char)*ptr);
	ptr++;

	// Packet ID
	pkt_index = ((unsigned char)*ptr);
	ptr++;

	// Query Length
	//len = (unsigned short)ptr[0] | ((unsigned short)ptr[1] << 8);
	//len = swap_short_from_little( ptr );
	debug( 1, "%04hx, %04hx", (unsigned short)ptr[0], ((unsigned short)ptr[1] << 8) );
	//len = (unsigned short)(unsigned short)ptr[0] | ((unsigned short)ptr[1] << 8);
	// TODO: fix this crap
	memcpy( &len, ptr+1, 1 );
	//memcpy( &len+1, ptr, 1 );
	//memcpy( &len, ptr, 2 );
	ptr += 2;

	debug( 1, "pkt_index = %d, pkt_max = %d, len = %d", pkt_index, pkt_max, len );
	if ( 0 != pkt_max )
	{
		// not a single packet response or callback
		debug( 2, "pkt_max %d", pkt_max );

		if ( 0 == pkt_index )
		{
			// to prevent reprocessing when we get the call back
			// override the packet flag so it looks like a single
			// packet response
			rawpkt[8] = '\0';
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
	process_haze_packet( server );
	return;
}

void deal_with_haze_status( struct qserver *server, char *rawpkt, int pktlen )
{
	char *pkt = rawpkt;
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
	change_server_port( server, atoi( pkt ), 0 );
	pkt += strlen( pkt ) + 1;

	cleanup_qserver( server, 1 );
}

int process_haze_packet( struct qserver *server )
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
		if ( pktlen < 12 )
		{
			// invalid packet
			malformed_packet( server, "too short" );
			cleanup_qserver( server, 1 );
			return 0;
		}

		// skip over the header
		//server->protocol_version = atoi( val+1 );
		ptr += 12;

		// 4 * null's signifies the end of a section

		// Basic Info
		while ( 0 == state && ptr < end )
		{
			// name value pairs null seperated
			char *var, *val;
			int var_len, val_len;

			if ( ptr+4 <= end && 0x00 == ptr[0] && 0x00 == ptr[1] && 0x00 == ptr[2] && 0x00 == ptr[3] )
			{
				// end of rules
				state++;
				ptr += 4;
				break;
			}

			var = ptr;
			var_len = strlen( var );
			ptr += var_len + 1;

			if ( ptr + 1 > end )
			{
				malformed_packet( server, "no basic value" );
				cleanup_qserver( server, 1);
				return 0;
			}

			val = ptr;
			val_len = strlen( val );
			ptr += val_len + 1;
			debug( 2, "var:%s (%d)=%s (%d)\n", var, var_len, val, val_len );

			// Lets see what we've got
			if ( 0 == strcmp( var, "serverName" ) )
			{
				server->server_name = strdup( val );
			}
			else if( 0 == strcmp( var, "map" ) )
			{
				server->map_name = strdup( val );
			}
			else if( 0 == strcmp( var, "maxPlayers" ) )
			{
				server->max_players = atoi( val );

			}
			else if( 0 == strcmp( var, "currentPlayers" ) )
			{
				server->num_players = no_players = atoi( val );
			}
			else
			{
				add_rule( server, var, val, NO_FLAGS );
			}
		}

		// rules
		while ( 1 == state && ptr < end )
		{
			// name value pairs null seperated
			char *var, *val;
			int var_len, val_len;

			if ( ptr+4 <= end && 0x00 == ptr[0] && 0x00 == ptr[1] && 0x00 == ptr[2] && 0x00 == ptr[3] )
			{
				// end of basic
				state++;
				ptr += 4;
				break;
			}
			var = ptr;
			var_len = strlen( var );
			ptr += var_len + 1;

			if ( ptr + 1 > end )
			{
				malformed_packet( server, "no basic value" );
				cleanup_qserver( server, 1);
				return 0;
			}

			val = ptr;
			val_len = strlen( val );
			ptr += val_len + 1;
			debug( 2, "var:%s (%d)=%s (%d)\n", var, var_len, val, val_len );

			// add the rule
			add_rule( server, var, val, NO_FLAGS );
		}

		// players
		while ( 2 == state && ptr < end )
		{
			// first we have the header
			char *header = ptr;
			int head_len = strlen( header );
			int header_type;
			ptr += head_len + 1;

			if ( ptr+2 <= end && 0x00 == ptr[0] && 0x00 == ptr[1] )
			{
				// end of player headers
				state++;
				ptr += 2;
				break;
			}

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
				malformed_packet( server, "no details for header '%s'", header );
				cleanup_qserver( server, 1 );
				return 0;
			}

		}

		while ( 3 == state && ptr < end )
		{
			char *header = ptr;
			int head_len = strlen( header );
			int header_type;
			// the next byte is the starting number
			total_players = *ptr++;

			if ( 0 == strcmp( header, "player_" ) || 0 == strcmp( header, "name_" ) )
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
					malformed_packet( server, "short player detail" );
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
					malformed_packet( server, "to many players %d > %d", total_players, no_players );
					cleanup_qserver( server, 1 );
					return 0;
				}
			}
		}

		if ( 3 == state )
		{
			no_teams = (unsigned char)*ptr;
			ptr++;

			debug( 2, "No teams:%d\n", no_teams );
			state = 3;
		}

		while ( 4 == state && ptr < end )
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
					malformed_packet( server, "short team detail" );
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

void send_haze_request_packet( struct qserver *server )
{
	char *packet;
	char query_buf[128];
	int len;
	unsigned char required = HAZE_BASIC_INFO;

	if ( get_server_rules )
	{
		required |= HAZE_GAME_RULES;
		server->flags |= TF_PLAYER_QUERY;
	}

	if ( get_player_info )
	{
		required |= HAZE_PLAYER_INFO;
		required |= HAZE_TEAM_INFO;
		server->flags |= TF_RULES_QUERY;
	}

	server->flags |= TF_STATUS_QUERY;

	if ( server->challenge )
	{
		// we've recieved a challenge response, send the query + challenge id
		len = sprintf(
			query_buf,
			"frdquery%c%c%c%c%c",
			(unsigned char)(server->challenge >> 24),
			(unsigned char)(server->challenge >> 16),
			(unsigned char)(server->challenge >> 8),
			(unsigned char)(server->challenge >> 0),
			required
		);
		packet = query_buf;
	}
	else
	{
		// Either basic v3 protocol or challenge request
		packet = haze_challenge;
		len = sizeof( haze_challenge );
	}

	send_packet( server, packet, len );
}
