/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * Teamspeak 3 query protocol
 * Copyright 2009 Steven Hartland
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

char *decode_val( char *val )
{
	val = str_replace( val, "\\\\", "\\" );
	val = str_replace( val, "\\/", "/" );
	val = str_replace( val, "\\s", " " );
	val = str_replace( val, "\\p", "|" );
	val = str_replace( val, "\\a", "\007" );
	val = str_replace( val, "\\b", "\010" );
	val = str_replace( val, "\\f", "\014" );
	val = str_replace( val, "\\n", "\012" );
	val = str_replace( val, "\\r", "\015" );
	val = str_replace( val, "\\t", "\011" );

	return str_replace( val, "\\v", "\013" );
}

// NOTE: replace must be smaller or equal in size to find
char *str_replace( char *source, char *find, char *replace )
{
	char *s = strstr( source, find );
	int rlen = strlen( replace );
	int flen = strlen( find );
	while( NULL != s )
	{
		strncpy( s, replace, rlen );
		strcpy( s + rlen, s + flen );
		s += rlen;
		s = strstr( s, find );
	}

	return source;
}

query_status_t send_ts3_request_packet( struct qserver *server )
{
	char buf[256];
	int serverport;

	debug( 3, "send_ts3_request_packet: state = %ld", server->challenge );

	switch ( server->challenge )
	{
	case 0:
		// Not seen a challenge yet, wait for it
		gettimeofday( &server->packet_time1, NULL);
		return INPROGRESS;

	case 1:
		// Select port
		serverport = get_param_i_value( server, "port", 0 ); 
		change_server_port( server, serverport, 1 );
		// NOTE: we use n_servers as an indeicated of how many responses we are expecting to get
		if ( get_player_info )
		{
			server->flags |= TF_PLAYER_QUERY|TF_RULES_QUERY;
			server->n_servers = 4;
		}
		else
		{
			server->flags |= TF_STATUS_QUERY;
			server->n_servers = 3;
		}
		sprintf( buf, "use port=%d\015\012", serverport );
		break;

	case 2:
		// Server Info
		sprintf( buf, "serverinfo\015\012" );
		break;

	case 3:
		// Player Info or Quit
		sprintf( buf, ( get_player_info ) ? "clientlist\015\012" : "quit\015\012" );
		break;

	case 4:
		// Quit or Done
		if ( get_player_info )	
		{
			sprintf( buf, "quit\015\012" );
		}
		else
		{
			return DONE_FORCE;
		}
		break;
	}
	server->saved_data.pkt_max = -1;

	return send_packet( server, buf, strlen( buf ) );
}

int valid_ts3_response( struct qserver *server, char *rawpkt, int pktlen )
{
	char *end = &rawpkt[pktlen-1];
	char *s = rawpkt;

	if ( 0 == strncmp( "TS3", s, 3 ) )
	{
		// Challenge
		return 1;
	}

	while ( NULL != s )
	{
		// use response
		if ( 0 == strncmp( "error", s, 5 ) )
		{
			// end cmd response
			if ( 0 == strncmp( "error id=0", s, 9 ) )
			{
				return 1;
			}
			else
			{
				// bad server
				server->server_name = DOWN;
				server->saved_data.pkt_index = 0;
				return DONE_FORCE;
			}
		}
		s = strstr( s, "\012\015" );
		if ( NULL != s )
		{
			s = ( s + 2 < end ) ? s + 2 : NULL;
		}
	}

	return 0;
}

query_status_t deal_with_ts3_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	char *s, *end, *player_name = "unknown";
	int valid_response = 0, mode = 0;
	char last_char;
	debug( 2, "processing..." );

	if ( 0 == pktlen )
	{
		// Invalid password
		return REQ_ERROR;
	}

	last_char = rawpkt[pktlen-1];
	rawpkt[pktlen-1] = '\0';
	end = &rawpkt[pktlen-1];
	s = rawpkt;

	debug( 3, "packet: combined = %d, challenge = %ld, n_servers = %d", server->combined, server->challenge, server->n_servers );
	if ( ! server->combined )
	{
		server->retry1 = n_retries;
		server->ping_total += time_delta( &packet_recv_time, &server->packet_time1 );
		server->n_requests++; // Not quite right but gives a good estimate

		valid_response = valid_ts3_response( server, rawpkt, pktlen );

		if ( 0 > valid_response )
		{
			// Error occured
			return valid_response;
		}

		// only update on the original packed not on a combined call
		server->challenge += valid_response;

		debug( 3, "new packet: valid_response = %d, challenge = %ld, n_servers = %d", valid_response, server->challenge, server->n_servers );

		if ( valid_response )
		{
			// Got a valid response, send the next request
			int ret = send_ts3_request_packet( server );
			if ( 0 != ret )
			{
				// error sending packet
				debug( 4, "Request failed: %d", ret );
				return ret;
			}
		}

		if ( server->n_servers >= server->challenge )
		{
			// response fragment recieved
			int pkt_id;
			int pkt_max;

			// We're expecting more to come
			debug( 5, "fragment recieved..." );
			pkt_id = packet_count( server );
			pkt_max = ( 0 == pkt_id ) ? 2 : pkt_id + 1;
			rawpkt[pktlen-1] = last_char; // restore the last character
			if ( ! add_packet( server, 0, pkt_id, pkt_max, pktlen, rawpkt, 1 ) )
			{
				// fatal error e.g. out of memory
				return MEM_ERROR;
			}

			// combine_packets will call us recursively
			return combine_packets( server );
		}
	}
	else if ( server->n_servers > server->challenge )
	{
		// recursive call which is still incomplete
		return INPROGRESS;
	}

	debug( 3, "processing response..." );

	s = strtok( rawpkt, "\012\015 |" );

	// NOTE: id=XXX and msg=XXX will be processed by the mod fillowing the one they where the response of
	while ( NULL != s )
	{
		debug( 6, "LINE: %s\n", s );
		switch ( mode )
		{
		case 0:
			// use response
			if ( 0 == strcmp( "TS3", s ) )
			{
				// nothing to do
			}
			else if ( 0 == strncmp( "error", s, 5 ) )
			{
				// end of use response
				mode++;
			}
			break;
		case 1:
			// serverinfo response
			if ( 0 == strncmp( "error", s, 5 ) )
			{
				// end of serverinfo response
				mode++;
			}
			else
			{
				// Server Rule
				char *key = s;
				char *value = strchr( key, '=' );
				if ( NULL != value )
				{
					*value = '\0';
					value++;
					debug( 6, "Rule: %s = %s\n", key, value );
					if ( 0 == strcmp( "virtualserver_name", key ) )
					{
						server->server_name = strdup( decode_val( value ) );
					}
					else if ( 0 == strcmp( "virtualserver_port", key ) )
					{
						change_server_port( server, atoi( value ), 0 );
						add_rule( server, key, value, NO_FLAGS );
					}
					else if ( 0 == strcmp( "virtualserver_maxclients", key ) )
					{
						server->max_players = atoi( value );
					}
					else if ( 0 == strcmp( "virtualserver_clientsonline", key ) )
					{
						server->num_players = atoi( value );
					}
					else if ( 0 == strcmp( "virtualserver_queryclientsonline", key ) )
					{
						// clientsonline includes queryclientsonline so remove these from our count
						server->num_players -= atoi( value );
					}
					else if ( 0 == strcmp( "virtualserver_status", key ) && ( 0 == strcmp( "virtual", value ) || 0 == strcmp( "none", value ) ) )
					{
						// Server is actually offline to client so display as down
						server->server_name = DOWN;
						server->saved_data.pkt_index = 0;
						return DONE_FORCE;
					}
					else
					{
						add_rule( server, key, value, NO_FLAGS);
					}
				}
			}
			break;

		case 2:
			// clientlist response
			if ( 0 == strncmp( "error", s, 5 ) )
			{
				// end of serverinfo response
				mode++;
			}
			else
			{
				// Client
				char *key = s;
				char *value = strchr( key, '=' );
				if ( NULL != value )
				{
					*value = '\0';
					value++;
					debug( 6, "Player: %s = %s\n", key, value );
					if ( 0 == strcmp( "client_nickname", key ) )
					{
						player_name = value;
					}
					else if ( 0 == strcmp( "clid", key ) )
					{
					}
					else if ( 0 == strcmp( "client_type", key ) && 0 == strcmp( "0", value ) )
					{
						struct player *player = add_player( server, server->n_player_info );
						if ( NULL != player )
						{
							player->name = strdup( decode_val( player_name ) );
						}
					}
				}
			}
			break;
		}
		s = strtok( NULL, "\012\015 |" );
	}

	gettimeofday( &server->packet_time1, NULL );

	server->map_name = strdup( "N/A" );
	return DONE_FORCE;
}

