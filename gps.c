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

int gps_max_players( struct qserver *server )
{
	struct player *player;
	int no_players = 0;
	if ( 0 == server->num_players )
	{
		return 0;
	}

	for ( player= server->players; player; player= player->next)
	{
		no_players++;
	}

	return ( no_players < server->num_players ) ? 1 : 0;
}

int gps_player_info_key( char *s, char *end)
{
    static char *keys[] = {
		"frags_", "team_", "ping_", "species_",
		"race_", "deaths_", "score_", "enemy_",
		"player_", "keyhash_", "teamname_",
		"playername_", "keyhash_", "kills_", "queryid"
	};
    int i;

    for  ( i= 0; i < sizeof(keys)/sizeof(char*); i++)
	{
		int len= strlen(keys[i]);
		if ( s+len < end && strncmp( s, keys[i], len) == 0)
		{
			return len;
		}
	}
    return 0;
}

void send_gps_request_packet( struct qserver *server )
{
    send_packet( server, server->type->status_packet, server->type->status_len );
}


void deal_with_gps_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	char *s, *key, *value, *end;
	struct player *player = NULL;
	int id_major=0, id_minor=0, final=0, player_num;
	char tmp[256];
	debug( 2, "processing..." );

	server->n_servers++;
	if ( server->server_name == NULL )
	{
		server->ping_total += time_delta( &packet_recv_time, &server->packet_time1 );
	}
	else
	{
		gettimeofday( &server->packet_time1, NULL );
	}

	/*
	// We're using the saved_data a bit differently here to track received
	// packets.
	// pkt_id is the id_major from the \queryid\
	// pkt_max is the total number of packets expected
	// pkt_index is a bit mask of the packets received.  The id_minor of
	// \queryid\ provides packet numbers (1 through pkt_max).
	*/
	if ( server->saved_data.pkt_index == -1)
	{
		server->saved_data.pkt_index= 0;
	}

	rawpkt[pktlen]= '\0';
	end= &rawpkt[pktlen];

	s= rawpkt;
	while ( *s)
	{
		// find the '\'
		while ( *s && *s == '\\')
		{
			s++;
		}

		if ( !*s )
		{
			// out of packet
			break;
		}
		// Start of key
		key = s;

		// while we still have data and its not a '\'
		while ( *s && *s != '\\')
		{
			s++;
		}

		if ( !*s )
		{
			// out of packet
			break;
		}

		// Terminate the key
		*s++= '\0';

		// Now for the value
		value = s;

		// while we still have data and its not a '\'
		while ( *s && *s != '\\')
		{
			s++;
		}

		if ( s[0] && s[1] )
		{
			//fprintf( stderr, "%s = %s\n", key, value );
			if ( ! isalpha((unsigned char)s[1]) )
			{
				// escape char?
				s++;
				// while we still have data and its not a '\'
				while ( *s && *s != '\\')
				{
					s++;
				}
			}
			else if (
				isalpha((unsigned char)s[1]) &&
				0 == strncmp( key, "player_", 7 ) &&
				0 != strcmp( key, "player_flags" )
			)
			{
				// possible '\' in player name
				if ( ! gps_player_info_key( s+1, end ) )
				{
					// yep there was an escape in the player name
					s++;
					// while we still have data and its not a '\'
					while ( *s && *s != '\\')
					{
						s++;
					}
				}
			}
		}

		if ( *s)
		{
			*s++= '\0';
		}

		//fprintf( stderr, "%s = %s\n", key, value );
		if ( *value == '\0')
		{
			if ( strcmp( key, "final" ) == 0 )
			{
				final= 1;
				if ( id_minor > server->saved_data.pkt_max )
				{
					server->saved_data.pkt_max = id_minor;
				}
				continue;
			}
		}

		/* This must be done before looking for player info because
		   "queryid" is a valid according to gps_player_info_key().
		*/
		if ( strcmp( key, "queryid") == 0)
		{
			sscanf( value, "%d.%d", &id_major, &id_minor);
			if ( server->saved_data.pkt_id == 0)
			{
				server->saved_data.pkt_id = id_major;
			}
			if ( id_major == server->saved_data.pkt_id)
			{
				if ( id_minor > 0)
				{
					// pkt_index is bitmask of packets recieved
					server->saved_data.pkt_index |= 1 << (id_minor-1);
				}
				if ( final && id_minor > server->saved_data.pkt_max)
				{
					server->saved_data.pkt_max = id_minor;
				}
			}
			continue;
		}

		if ( player == NULL )
		{
			int len = gps_player_info_key( key, end);
			if ( len )
			{
				// We have player info
				int player_number = atoi( key + len );
				player = get_player_by_number( server, player_number);

				// && gps_max_players( server ) due to bf1942 issue
				// where the actual no players is correct but more player
				// details are returned
				if ( player == NULL && gps_max_players( server ) )
				{
					player = add_player( server, player_number );
					if ( player )
					{
						// init to -1 so we can tell if
						// we have team info
						player->team = -1;
						player->deaths = -999;
					}
				}
			}
		}

		if ( strcmp( key, "mapname") == 0 && !server->map_name)
		{
			server->map_name= strdup( value );
		}
		else if ( strcmp( key, "hostname") == 0 && !server->server_name)
		{
			server->server_name= strdup( value );
		}
		else if ( strcmp( key, "hostport") == 0)
		{
			change_server_port( server, atoi( value), 0 );
		}
		else if ( strcmp( key, "maxplayers") == 0)
		{
			server->max_players= atoi( value );
		}
		else if ( strcmp( key, "numplayers") == 0)
		{
			server->num_players= atoi( value);
		}
		else if ( strcmp( key, server->type->game_rule) == 0 && !server->game)
		{
			server->game= strdup( value );
			add_rule( server, key, value, NO_FLAGS);
		}
		else if ( strcmp( key, "final") == 0)
		{
			final= 1;
			if ( id_minor > server->saved_data.pkt_max)
			{
				server->saved_data.pkt_max = id_minor;
			}
			continue;
		}
		else if ( strncmp( key, "player_", 7) == 0  || strncmp( key, "playername_", 11) == 0 )
		{
			int no;
			if ( strncmp( key, "player_", 7) == 0 )
			{
				no = atoi(key+7);
			}
			else
			{
				no = atoi(key+11);
			}

			if ( player && player->number == no )
			{
				player->name = strdup( value);
				player = NULL;
			}
			else if ( NULL != ( player = get_player_by_number( server, no ) ) )
			{
				player->name = strdup( value);
				player = NULL;
			}
			else if ( gps_max_players( server ) )
			{
				// gps_max_players( server ) due to bf1942 issue
				// where the actual no players is correct but more player
				// details are returned
				player = add_player( server, no );
				if ( player )
				{
					player->name= strdup( value);
					// init to -1 so we can tell if
					// we have team info
					player->team = -1;
					player->deaths = -999;
				}
			}
		}
		else if ( strncmp( key, "teamname_", 9) == 0 )
		{
			// Yes plus 1 BF1942 is a silly
			players_set_teamname( server, atoi( key+9 ) + 1, value );
		}
		else if ( strncmp( key, "team_t", 6) == 0 )
		{
			players_set_teamname( server, atoi( key+6 ), value );
		}
		else if ( strncmp( key, "frags_", 6) == 0 )
		{
			player = get_player_by_number( server, atoi( key+6 ) );
			if ( NULL != player )
			{
				player->frags= atoi( value );
			}
		}
		else if ( strncmp( key, "kills_", 6) == 0 )
		{
			player = get_player_by_number( server, atoi( key+6 ) );
			if ( NULL != player )
			{
				player->frags= atoi( value );
			}
		}
		else if ( strncmp( key, "team_", 5) == 0 )
		{
			player = get_player_by_number( server, atoi( key+5 ) );
			if ( NULL != player )
			{
				if ( ! isdigit( (unsigned char)*value ) )
				{
					player->team_name= strdup(value);
				}
				else
				{
					player->team= atoi( value);
				}
				server->flags|= FLAG_PLAYER_TEAMS;
			}
		}
		else if ( strncmp( key, "skin_", 5) == 0 )
		{
			player = get_player_by_number( server, atoi( key+5 ) );
			if ( NULL != player )
			{
				player->skin= strdup( value);
			}
		}
		else if ( strncmp( key, "mesh_", 5) == 0 )
		{
			player = get_player_by_number( server, atoi( key+5 ) );
			if ( NULL != player )
			{
				player->mesh = strdup( value);
			}
		}
		else if ( strncmp( key, "ping_", 5) == 0 )
		{
			player = get_player_by_number( server, atoi( key+5 ) );
			if ( NULL != player )
			{
				player->ping= atoi( value);
			}
		}
		else if ( strncmp( key, "face_", 5) == 0 )
		{
			player = get_player_by_number( server, atoi( key+5 ) );
			if ( NULL != player )
			{
				player->face= strdup( value);
			}
		}
		else if ( strncmp( key, "deaths_", 7) == 0 )
		{
			player = get_player_by_number( server, atoi( key+7 ) );
			if ( NULL != player )
			{
				player->deaths= atoi( value );
			}
		}
		// isnum( key[6] ) as halo uses score_tX for team scores
		else if ( strncmp( key, "score_", 6) == 0 && isdigit( (unsigned char)key[6] ) )
		{
			player = get_player_by_number( server, atoi( key+6 ) );
			if ( NULL != player )
			{
				player->score = atoi( value );
			}
		}
		else if ( player && strncmp( key, "playertype", 10) == 0)
		{
			player->team_name= strdup( value );
		}
		else if ( player && strncmp( key, "charactername", 13) == 0)
		{
			player->face = strdup( value );
		}
		else if ( player && strncmp( key, "characterlevel", 14) == 0)
		{
			player->ship= atoi( value );
		}
		else if ( strncmp( key, "keyhash_", 8) == 0)
		{
			// Ensure these dont make it into the rules
		}
		else if ( 2 == sscanf( key, "%255[^_]_%d", tmp, &player_num ) )
		{
			// arbitary player info
			player = get_player_by_number( server, player_num );
			if ( NULL != player )
			{
				player_add_info( player, tmp, value, NO_FLAGS );
			}
			else if ( gps_max_players( server ) )
			{
				// gps_max_players( server ) due to bf1942 issue
				// where the actual no players is correct but more player
				// details are returned
				player = add_player( server, player_num );

				if ( player )
				{
					player->name = NULL;
					// init to -1 so we can tell if
					// we have team info
					player->team = -1;
					player->deaths = -999;
				}
				player_add_info( player, tmp, value, NO_FLAGS );
			}
		}
		else
		{
			player = NULL;
			add_rule( server, key, value, NO_FLAGS);
		}
	}

	debug( 2, "final %d\n", final );
	debug( 2, "pkt_id %d\n", server->saved_data.pkt_id );
	debug( 2, "pkt_max %d\n", server->saved_data.pkt_max );
	debug( 2, "pkt_index %x\n", server->saved_data.pkt_index );

	if (
		( final && server->saved_data.pkt_id == 0 ) ||
		( server->saved_data.pkt_max && server->saved_data.pkt_index >= ((1<<(server->saved_data.pkt_max))-1) ) ||
		( server->num_players < 0 && id_minor >= 3)
	)
	{
		cleanup_qserver( server, 1);
	}
}
