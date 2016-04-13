/*
 * qstat
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
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "debug.h"
#include "qstat.h"
#include "utils.h"
#include "packet_manip.h"

char *decode_ts3_val( char *val )
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

int all_ts3_servers( struct qserver *server )
{
	return ( 1 == get_param_i_value( server, "allservers", 0 ) ) ? 1 : 0;
}

query_status_t send_ts3_all_servers_packet( struct qserver *server )
{
	char buf[256], *password, *username;
	switch ( server->challenge )
	{
	case 0:
		// Not seen a challenge yet, wait for it
		server->n_servers = 999;
		return INPROGRESS;

	case 1:
		password = get_param_value( server, "password", "" );
		if ( 0 != strlen( password ) )
		{
			username = get_param_value( server, "username", "serveradmin" );
			sprintf( buf, "login %s %s\015\012", username, password );
			break;
		}
		// NOTE: no break so we fall through
		server->challenge++;

	case 2:
		// NOTE: we currently don't support player info
		server->flags |= TF_STATUS_QUERY;
		server->n_servers = 3;
		sprintf( buf, "serverlist\015\012" );
		break;

	case 3:
		sprintf( buf, "quit\015\012" );
		break;

	case 4:
		return DONE_FORCE;
	}

	server->saved_data.pkt_max = -1;

	return send_packet( server, buf, strlen( buf ) );
}


query_status_t send_ts3_single_server_packet( struct qserver *server )
{
	char buf[256], *password, *username;
	int serverport;
	switch ( server->challenge )
	{
	case 0:
		// Not seen a challenge yet, wait for it
		server->n_servers = 999;
		return INPROGRESS;

	case 1:
		// Login if needed
		password = get_param_value( server, "password", "" );
		if ( 0 != strlen( password ) )
		{
			username = get_param_value( server, "username", "serveradmin" );
			sprintf( buf, "login %s %s\015\012", password, username );
			break;
		}
		// NOTE: no break so we fall through
		server->challenge++;

	case 2:
		// Select port
		serverport = get_param_i_value( server, "port", 0 ); 
		change_server_port( server, serverport, 1 );
		// NOTE: we use n_servers as an indication of how many responses we are expecting to get
		if ( get_player_info )
		{
			server->flags |= TF_PLAYER_QUERY|TF_RULES_QUERY;
			server->n_servers = 5;
		}
		else
		{
			server->flags |= TF_STATUS_QUERY;
			server->n_servers = 4;
		}
		sprintf( buf, "use port=%d\015\012", serverport );
		break;

	case 3:
		// Server Info
		sprintf( buf, "serverinfo\015\012" );
		break;

	case 4:
		// Player Info, Quit or Done
		sprintf( buf, ( get_player_info ) ? "clientlist\015\012" : "quit\015\012" );
		break;

	case 5:
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

query_status_t send_ts3_request_packet( struct qserver *server )
{
	debug( 3, "send_ts3_request_packet: state = %ld", server->challenge );
	return ( all_ts3_servers( server ) ) ? send_ts3_all_servers_packet( server ) : send_ts3_single_server_packet( server );
}

int valid_ts3_response( struct qserver *server, char *rawpkt, int pktlen )
{
	char *end = &rawpkt[pktlen-1];
	char *s = rawpkt;

	if ( 0 == strncmp( "TS3", s, 3 ) )
	{
		// Challenge
		server->master_pkt_len = 3;
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
				server->master_pkt_len = s - rawpkt + 9;
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
	char *s, *player_name = "unknown";
	int valid_response = 0, mode = 0, all_servers = 0;
	char last_char;
	unsigned short port = 0, down = 0, auth_seen = 0;
	debug( 2, "processing..." );

	if ( 0 == pktlen )
	{
		// Invalid password
		return REQ_ERROR;
	}

	last_char = rawpkt[pktlen-1];
	rawpkt[pktlen-1] = '\0';
	s = rawpkt;
	all_servers = all_ts3_servers( server );

	debug( 3, "packet: combined = %d, challenge = %ld, n_servers = %d", server->combined, server->challenge, server->n_servers );
	if ( ! server->combined )
	{
		server->retry1 = n_retries;
		if ( 0 == server->n_requests )
		{
			server->ping_total = time_delta( &packet_recv_time, &server->packet_time1 );
			server->n_requests++;
		}

		if ( server->n_servers >= server->challenge )
		{
			// response fragment recieved
			int pkt_id;
			int pkt_max;

			// We're expecting more to come
			debug( 5, "fragment recieved..." );
			pkt_id = packet_count( server );
			pkt_max = pkt_id + 1;
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
	else
	{
		valid_response = valid_ts3_response( server, rawpkt + server->master_pkt_len, pktlen - server->master_pkt_len );

		debug(2, "combined packet: valid_response: %d, challenge: %ld, n_servers: %d, offset: %d", valid_response, server->challenge, server->n_servers, server->master_pkt_len );

		if ( 0 > valid_response )
		{
			// Error occured
			return valid_response;
		}

		server->challenge += valid_response;

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

		if ( server->n_servers > server->challenge ) {
			// recursive call which is still incomplete
			return INPROGRESS;
		}
	}

	// Correct ping
	// Not quite right but gives a good estimate
	server->ping_total = ( server->ping_total * server->n_requests ) / 2;

	debug( 3, "processing response..." );

	s = strtok( rawpkt, "\012\015 |" );

	// NOTE: id=XXX and msg=XXX will be processed by the mod following the one they where the response of
	while ( NULL != s )
	{
		debug( 4, "LINE: %d, %s\n", mode, s );
		switch ( mode )
		{
		case 0:
			// prompt, use or serverlist response
			if ( 0 == strcmp( "TS3", s ) )
			{
				// nothing to do unless in all servers mode
				if ( 1 == all_servers )
				{
					mode++;
				}
			}
			else if ( 0 == strncmp( "error", s, 5 ) )
			{
				// end of use response
				mode++;
			}
			break;

		case 1:
			// serverinfo or serverlist response including condition authentication
			if ( 0 == auth_seen && 0 != strlen( get_param_value( server, "password", "" ) ) && 0 == strncmp( "error", s, 5 ) )
			{
				// end of auth response
				auth_seen = 1;
			}
			else if ( 0 == strncmp( "error", s, 5 ) )
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
						if ( 1 == all_servers )
						{
							struct qserver *new_server = add_qserver_byaddr( ntohl( server->ipaddr ), port, server->type, NULL );
							if ( NULL != new_server )
							{
								if ( down )
								{
									// Status indicates this server is actually offline
									new_server->server_name = DOWN;
								}
								else
								{
									new_server->max_players = server->max_players;
									new_server->num_players = server->num_players;
									new_server->server_name = strdup( decode_ts3_val( value ) );
									new_server->map_name = strdup( "N/A" );
									new_server->ping_total = server->ping_total;
									new_server->n_requests = server->n_requests;
								}
								cleanup_qserver( new_server, FORCE );
							}
							down = 0;
						}
						else
						{
							server->server_name = strdup( decode_ts3_val( value ) );
						}
					}
					else if ( 0 == strcmp( "virtualserver_port", key ) )
					{
						port = atoi( value );
						change_server_port( server, port, 0 );
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
					else if ( 0 == strcmp( "virtualserver_status", key ) && 0 != strcmp( "online", value ) )
					{
						// Server is actually offline to client so display as down
						down = 1;
						if ( 1 != all_servers )
						{
							server->server_name = DOWN;
							//server->saved_data.pkt_index = 0;
							return DONE_FORCE;
						}
					}
					else if ( 0 == strcmp( "id", key ) || 0 == strcmp( "msg", key ) )
					{
						// Ignore details from the response code
					}
					else if ( 1 != all_servers )
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
							player->name = strdup( decode_ts3_val( player_name ) );
						}
					}
					else if ( 0 == strcmp( "id", key ) || 0 == strcmp( "msg", key ) )
					{
						// Ignore details from the response code
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

