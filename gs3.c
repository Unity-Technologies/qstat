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

#include <stdlib.h>
#include <stdio.h>

#include "debug.h"
#include "qstat.h"
#include "packet_manip.h"

void deal_with_gs3_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	char *ptr = rawpkt;
	char *end = rawpkt + pktlen;
	unsigned char type = 0;
	unsigned char no_players = 0;
	unsigned char total_players = 0;
	unsigned char no_teams = 0;
	unsigned char total_teams = 0;
	unsigned int pkt_id;
	unsigned char pkt_index;
	unsigned char flag;

	debug( 2, "processing packet..." );

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
		fprintf( stderr, "Invalid packet ( missing splitnum )\n" );
		cleanup_qserver( server, 1 );
		return;
	}
	ptr += 9;

	flag = *ptr++;
	pkt_index = *ptr++;

	debug( 3, "splitnum 0x%hhx, %d", flag, pkt_index );
	if ( 0x80 != flag )
	{
		// not the final packet
		SavedData *sdata;

		if ( server->saved_data.data == NULL )
		{
			debug( 2, "first packet" );
			sdata = &server->saved_data;
			sdata->pkt_max = ( pkt_index ) ? pkt_index : 2; // <- min max will alter later
		}
		else
		{
			debug( 2, "another packet" );
			sdata = (SavedData*) calloc( 1, sizeof(SavedData));
			sdata->next = server->saved_data.next;
			server->saved_data.next = sdata;
			if ( pkt_index >= sdata->pkt_max )
			{
				sdata->pkt_max = pkt_index + 1;
				if (
					0x00 != rawpkt[pktlen-3] ||
					0x00 != rawpkt[pktlen-2] ||
					0x00 != rawpkt[pktlen-1]
				)
				{
					// Guess that we have more to come
					debug( 2, "more to come 0x%hxx 0x%hhx 0x%hhx", rawpkt[pktlen-3], rawpkt[pktlen-2], rawpkt[pktlen-1] );
					sdata->pkt_max++;
				}
				else
				{
					debug( 2, "pkt_max %d", sdata->pkt_max );
				}
			}
		}

		sdata->pkt_index = pkt_index;
		sdata->pkt_id = pkt_id;
		sdata->datalen = pktlen;
		if ( 0 != pkt_index )
		{
			// this isnt the first packet chop off the header
			int header_len = strlen( ptr );
			debug( 2, "dup header %s (%d)", ptr, header_len );
			ptr += header_len + 2;
			sdata->datalen -= 15 + header_len + 2;
		}
		else
		{
			// this is the first packet we want it all except the final byte
			ptr = rawpkt;
			sdata->datalen--;

			// to prevent reprocessing when we get the call back
			// override the packet flag so it looks like a single
			// packet response
			ptr[14] = 0x80;
		}

		sdata->data = (char*)malloc( sdata->datalen );
		if ( NULL == sdata->data )
		{
			fprintf( stderr, "Out of memory\n" );
			cleanup_qserver( server, 1 );
			return;
		}

		memcpy( sdata->data, ptr, sdata->datalen );

		// combine_packets will call us recursively
		combine_packets( server );
		return;
	}

	// if we get here we have what should be a full packet
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
			fprintf( stderr, "Invalid packet detected (no rule value)\n" );
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
		else if ( 0 == var_len )
		{
			// check for end of section
			ptr -= val_len;
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
		fprintf( stderr, "Invalid packet detected (no player headers)\n" );
		cleanup_qserver( server, 1);
		return;
	}

	if ( ptr >= end )
	{
		fprintf( stderr, "Invalid packet detected (no player headers)\n" );
		cleanup_qserver( server, 1);
		return;
	}

	while ( 1 == type && ptr < end )
	{
		// first we have the header
		char *header = ptr;
		int head_len = strlen( header );
		ptr += head_len + 1;

		if ( 0 == head_len )
		{
			// no more info
			debug( 3, "All done" );
			cleanup_qserver( server, 1 );
			return;
		}

		debug( 2, "player header '%s'", header );
		total_players = 0;

		// the next byte is the starting number
		// but we dont need it due to the way we combine packets
		ptr++;

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
				if ( total_players != no_players )
				{
					fprintf( stderr, "Invalid packet detected (bad number of players %d != %d)\n", total_players, no_players );
					cleanup_qserver( server, 1 );
					return;
				}

				if ( 0x00 == *ptr )
				{
					debug( 3, "end of players" );
					// end of all player headers / detail
					type = 2;
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

			debug( 2, "Player[%d][%s]=%s\n", total_players, header, val );

			total_players++;

			if ( total_players > no_players )
			{
				fprintf( stderr, "Invalid packet detected (to many players %d > %d)\n", total_players, no_players );
				cleanup_qserver( server, 1 );
				return;
			}
		}
	}

	if ( 2 != type )
	{
		// no more info should be team info here as we
		// requested it
		fprintf( stderr, "Invalid packet detected (no teams)\n" );
		cleanup_qserver( server, 1 );
		return;
	}

	no_teams = (unsigned char)*ptr;
	ptr++;

	debug( 2, "No teams:%d\n", no_teams );

	while ( ptr < end )
	{
		// first we have the header
		char *header = ptr;
		int head_len = strlen( header );
		ptr += head_len + 1;

		if ( 0 == head_len )
		{
			// no more info
			debug( 3, "All done" );
			cleanup_qserver( server, 1 );
			return;
		}

		debug( 2, "team header '%s'", header );
		total_teams = 0;

		// the next byte is the starting number
		// but we dont need it due to the way we combine packets
		ptr++;

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
				return;
			}
			val = ptr;
			val_len = strlen( val );
			ptr += val_len + 1;

			// lets see what we got
			if ( 0 == strcmp( header, "team_t" ) )
			{
				// BF being stupid again teams 1 based instead of 0
				players_set_teamname( server, total_teams + 1, val );
			}
			sprintf( rule, "%s%d", header, total_teams );
			add_rule( server, rule, val, NO_FLAGS );
			debug( 2, "Team[%d][%s]=%s\n", total_teams, header, val );

			total_teams++;
			if ( 0x00 == *ptr )
			{
				// end of this headers teams
				ptr++;
				break;
			}
		}
	}

	cleanup_qserver( server, 1 );
	return;
}
