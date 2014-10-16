/*
 * qstat
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
#include <netinet/in.h>
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

query_status_t deal_with_gs3_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	char *ptr = rawpkt;
	unsigned int pkt_id;
	int pkt_index;
	unsigned char flag;
	unsigned int pkti, final;

	debug( 2, "packet n_requests %d, retry1 %d, n_retries %d, delta %d", server->n_requests, server->retry1, n_retries, time_delta( &packet_recv_time, &server->packet_time1 ) );
	server->ping_total += time_delta( &packet_recv_time, &server->packet_time1 );

	if ( 7 == pktlen && 0x09 == *ptr )
	{
		// gs4 query sent to a gs3 server
		// switch protocols and try again

		// Correct the stats due to two phase protocol
		int i;
		server_type *gs3_type = &builtin_types[50];
		debug( 3, "Attempting gs3 fallback from gs4" );
		server->retry1++;
		if ( GAMESPY3_PROTOCOL_SERVER != gs3_type->id )
		{
			// Static coding failure do it the hard way
			debug( 1, "gs3 static lookup failure, using dynamic lookup" );
			for( i = 0; i < GAMESPY3_PROTOCOL_SERVER; i++ )
			{
				if ( GAMESPY3_PROTOCOL_SERVER == builtin_types[i].id )
				{
					// found it
					gs3_type = &builtin_types[i];
					i = GAMESPY3_PROTOCOL_SERVER;
				}
			}

			if ( GAMESPY3_PROTOCOL_SERVER != gs3_type->id )
			{
				malformed_packet( server, "GS3 protocol not found" );
				return PKT_ERROR;
			}
		}
		server->type = gs3_type;
		return send_gs3_request_packet( server );
	}

	if ( pktlen < 12 )
	{
		// invalid packet?
		malformed_packet( server, "too short" );
		return PKT_ERROR;
	}

	if ( 0x09 == *ptr )
	{
		// challenge response
		// Could check the header here should
		// match the 4 byte id sent
		ptr++;
		memcpy( &pkt_id, ptr, 4 );
		ptr += 4;
		server->challenge = atoi( ptr );
		debug( 3, "Challenge: %ld", server->challenge );
		server->retry1++;

		return send_gs3_request_packet( server );
	}

	if ( 0x00 != *ptr )
	{
		malformed_packet( server, "bad initial byte '%hhx'", *ptr );
		return PKT_ERROR;
	}
	ptr++;

	server->n_servers++;

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
			return deal_with_gs3_status( server, ptr, pktlen - ( ptr - rawpkt )  );
		}
		else
		{
			malformed_packet( server, "missing splitnum" );
			return PKT_ERROR;
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
			debug( 2, "more to come 0x%hhx 0x%hhx 0x%hhx", rawpkt[pktlen-3], rawpkt[pktlen-2], rawpkt[pktlen-1] );
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
			return MEM_ERROR;
		}

		// combine_packets will call us recursively
		return combine_packets( server );
	}

	// if we get here we have what should be a full packet
	return process_gs3_packet( server );
}

query_status_t deal_with_gs3_status( struct qserver *server, char *rawpkt, int pktlen )
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
	// UT3 retail compatibility
	if ( 0 == strncmp( pkt, "OwningPlayerId", 13 ) )
	{
		char *end = pkt + strlen( pkt ) + 1;
		char *var = pkt;
		while ( NULL != var )
		{
			char *next;
			char *val = strstr( var, "=" );
			*val = '\0';
			val++;
			next = strstr( val, "," );
			if ( NULL != next )
			{
				*next = '\0';
				next++;
			}

			if ( 0 == strcmp( var, "mapname" ) )
			{
				if ( server->map_name )
				{
					free( server->map_name );
				}
				server->map_name = strdup( val );
			}
			else if ( 0 == strcmp( var, "p1073741825" ) )
			{
				if ( server->map_name )
				{
					free( server->map_name );
				}
				server->map_name = strdup( val );
			}
			else if ( 0 == strcmp( var, "p1073741826" ) )
			{
				add_rule( server, "gametype", val, OVERWITE_DUPLICATES );
			}
			else if ( 0 == strcmp( var, "p268435705" ) )
			{
				add_rule( server, "timelimit", val, OVERWITE_DUPLICATES );
			}
			else if ( 0 == strcmp( var, "p1073741827" ) )
			{
				add_rule( server, "description", val, OVERWITE_DUPLICATES );
#ifndef UT3_PATCHED
				if ( 0 != strlen( val ) )
				{
					if ( server->server_name )
					{
						char *name = (char*)realloc( server->server_name, strlen( server->server_name ) + strlen( val ) + 3 );
						if ( name )
						{
							strcat( name, ": " );
							strcat( name, val );
							server->server_name = name;
						}
					}
					server->server_name = strdup( val );
				}
#endif
			}
			else if ( 0 == strcmp( var, "p268435704" ) )
			{
				add_rule( server, "goalscore", val, OVERWITE_DUPLICATES );
			}
			else if ( 0 == strcmp( var, "s32779" ) )
			{
				add_rule( server, "gamemode", val, OVERWITE_DUPLICATES );
			}
			else
			{
				add_rule( server, var, val, OVERWITE_DUPLICATES );
			}

			var = next;
		}
		pkt = end;
	}
	else
	{
		server->map_name = strdup( pkt );
		pkt += strlen( pkt ) + 1;
	}

	// num players
	server->num_players = atoi( pkt );
	pkt += strlen( pkt ) + 1;

	// max_players
	server->max_players = atoi( pkt );
	pkt += strlen( pkt ) + 1;

	// hostport
	change_server_port( server, atoi( pkt ), 0 );
	pkt += strlen( pkt ) + 1;

	return DONE_FORCE;
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

		if( 6 <= get_debug_level() )
		{
			print_packet( server, ptr, pktlen );
		}

		// check we have a full header
		if ( pktlen < 16 )
		{
			// invalid packet?
			malformed_packet( server, "too short" );
			return PKT_ERROR;
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

			if ( ptr + 1 >= end )
			{
				debug( 4, "state = %d, %hhx, bytes left = %d", state, ptr[0], (int)(end - ptr) );
			}
			else
			{
				debug( 4, "state = %d, %hhx, %hhx, bytes left = %d", state, ptr[0], ptr[1], (int)(end - ptr) );
			}
			if ( 0x00 == ptr[0] && ( ptr + 1 >= end || 0x01 >= ptr[1] ) )
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
				malformed_packet( server, "no rule value" );
				return PKT_ERROR;
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
				add_rule( server, var, val, NO_FLAGS );
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
				// UT3 retail compatibility
				if ( 0 == strncmp( val, "OwningPlayerId", 13 ) )
				{
					var = val;
					while ( NULL != var )
					{
						char *next;
						char *val = strstr( var, "=" );
						*val = '\0';
						val++;
						next = strstr( val, "," );
						if ( NULL != next )
						{
							*next = '\0';
							next++;
						}

						if ( 0 == strcmp( var, "mapname" ) )
						{
							if ( server->map_name )
							{
								free( server->map_name );
							}
							server->map_name = strdup( val );
						}
						else if ( 0 == strcmp( var, "p1073741825" ) )
						{
							if ( server->map_name )
							{
								free( server->map_name );
							}
							server->map_name = strdup( val );
						}
						else if ( 0 == strcmp( var, "p1073741826" ) )
						{
							add_rule( server, "gametype", val, OVERWITE_DUPLICATES );
						}
						else if ( 0 == strcmp( var, "p268435705" ) )
						{
							add_rule( server, "timelimit", val, OVERWITE_DUPLICATES );
						}
						else if ( 0 == strcmp( var, "p1073741827" ) )
						{
#ifndef UT3_PATCHED
							if ( 0 != strlen( val ) )
							{
								if ( server->server_name )
								{
									char *name = (char*)realloc( server->server_name, strlen( server->server_name ) + strlen( val ) + 3 );
									if ( name )
									{
										strcat( name, ": " );
										strcat( name, val );
										server->server_name = name;
									}
								}
								server->server_name = strdup( val );
							}
#endif
						}
						else if ( 0 == strcmp( var, "p268435704" ) )
						{
							add_rule( server, "goalscore", val, OVERWITE_DUPLICATES );
						}
						else if ( 0 == strcmp( var, "s32779" ) )
						{
							add_rule( server, "gamemode", val, OVERWITE_DUPLICATES );
						}
						else
						{
							add_rule( server, var, val, OVERWITE_DUPLICATES );
						}
						var = next;
					}
				}
				else
				{
					server->map_name = strdup( val );
				}
			}
			else if( 0 == strcmp( var, "map" ) )
			{
				// BF2MC compatibility
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
				change_server_port( server, atoi( val ), 0 );
			}
			else if ( 0 == strcmp( var, "p1073741825" ) )
			{
				// UT3 demo compatibility
				if ( server->map_name )
				{
					free( server->map_name );
				}
				server->map_name = strdup( val );
			}
			else if ( 0 == strcmp( var, "p1073741826" ) )
			{
				// UT3 demo compatibility
				add_rule( server, "gametype", val, OVERWITE_DUPLICATES );
			}
			else if ( 0 == strcmp( var, "p268435705" ) )
			{
				// UT3 demo compatibility
				add_rule( server, "timelimit", val, OVERWITE_DUPLICATES );
			}
			else if ( 0 == strcmp( var, "p1073741827" ) )
			{
				// UT3 demo compatibility
				add_rule( server, "description", val, OVERWITE_DUPLICATES );
#ifndef UT3_PATCHED
				if ( 0 != strlen( val ) )
				{
					if ( server->server_name )
					{
						char *name = (char*)realloc( server->server_name, strlen( server->server_name ) + strlen( val ) + 3 );
						if ( name )
						{
							strcat( name, ": " );
							strcat( name, val );
							server->server_name = name;
						}
					}
					server->server_name = strdup( val );
				}
#endif
			}
			else if ( 0 == strcmp( var, "p268435704" ) )
			{
				// UT3 demo compatibility
				add_rule( server, "goalscore", val, OVERWITE_DUPLICATES );
			}
			else if ( 0 == strcmp( var, "s32779" ) )
			{
				// UT3 demo compatibility
				add_rule( server, "gamemode", val, OVERWITE_DUPLICATES );
			}
			else
			{
				add_rule( server, var, val, OVERWITE_DUPLICATES );
			}
		}


		while ( 1 == state && ptr < end )
		{
			// first we have the header
			char *header = ptr;
			int head_len = strlen( header );
			int header_type;

			debug( 4, "state = %d, bytes left = %d, head_len = %d", state, (int)(end - ptr), head_len );

			if ( 0 == head_len )
			{
				// no more info
				debug( 3, "All done" );
				return DONE_FORCE;
			}

			ptr += head_len + 1;

			if ( ptr >= end )
			{
				// partial header, should be restarted in the next fragment
				debug( 5, "partial header '%s'", header );
				// ensure gt than
				ptr++;

				break;
			}

			debug( 2, "player header '%s'", header );

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
					debug( 3, "end of '%s' detail, %d bytes left", header, (int)(end - ptr) );
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
					return PKT_ERROR;
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
					return PKT_ERROR;
				}
			}
		}

		debug( 4, "state = %d, bytes left = %d", state, (int)(end - ptr) );

		if ( 2 == state && ptr < end )
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
				return DONE_FORCE;
			}

			if ( ptr >= end )
			{
				// partial header, should be restarted in the next fragment
				debug( 5, "partial header '%s'", header );
				break;
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
					return PKT_ERROR;
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

	return DONE_FORCE;
}

query_status_t send_gs3_request_packet( struct qserver *server )
{
	char *packet;
	char query_buf[128];
	int len;

	// In the old v3 protocol the doesnt seems to make a difference
	// to what the servers sends but in the challenge version it definitely does
	if ( get_player_info || get_server_rules )
	{
		server->flags |= TF_PLAYER_QUERY|TF_RULES_QUERY;
		if ( server->challenge )
		{
			// we've recieved a challenge response, send the query + challenge id
			len = sprintf(
				query_buf,
				"\xfe\xfd%c\x10\x20\x30\x40%c%c%c%c\xff\xff\xff\x01",
				0x00,
				(unsigned char)(server->challenge >> 24),
				(unsigned char)(server->challenge >> 16),
				(unsigned char)(server->challenge >> 8),
				(unsigned char)(server->challenge >> 0)
			);
			packet = query_buf;
		}
		else
		{
			// Either basic v3 protocol or challenge request
			packet = server->type->player_packet;
			len = server->type->player_len;
		}
	}
	else
	{
		server->flags |= TF_STATUS_QUERY;
		if ( server->challenge )
		{
			// we've recieved a challenge response, send the query + challenge id
			len = sprintf(
				query_buf,
				"\xfe\xfd%c\x10\x20\x30\x40%c%c%c%c\x06\x01\x06\x05\x08\x0a\x04%c%c",
				0x00,
				(unsigned char)(server->challenge >> 24),
				(unsigned char)(server->challenge >> 16),
				(unsigned char)(server->challenge >> 8),
				(unsigned char)(server->challenge >> 0),
				0x00,
				0x00
			);
			packet = query_buf;
		}
		else
		{
			// Either basic v3 protocol or challenge request
			packet = server->type->status_packet;
			len = server->type->status_len;
		}
	}

	return send_packet( server, packet, len );
}
